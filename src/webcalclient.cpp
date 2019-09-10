/*
 * This file is part of buteo-sync-plugin-webcal package
 *
 * Copyright (C) 2019 Damien Caliste <dcaliste@free.fr>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "webcalclient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDateTime>
#include <QtGlobal>

#include <PluginCbInterface.h>
#include <LogMacros.h>
#include <ProfileEngineDefs.h>
#include <ProfileManager.h>

#include <icalformat.h>

extern "C" WebCalClient* createPlugin(const QString& aPluginName,
                                      const Buteo::SyncProfile& aProfile,
                                      Buteo::PluginCbInterface *aCbInterface)
{
    return new WebCalClient(aPluginName, aProfile, aCbInterface);
}

extern "C" void destroyPlugin(WebCalClient *aClient)
{
    delete aClient;
}

WebCalClient::WebCalClient(const QString& aPluginName,
                           const Buteo::SyncProfile& aProfile,
                           Buteo::PluginCbInterface *aCbInterface)
    : ClientPlugin(aPluginName, aProfile, aCbInterface)
    , mNAManager(new QNetworkAccessManager)
    , mCalendar(0)
    , mStorage(0)
{
}

WebCalClient::~WebCalClient()
{
    delete(mNAManager);
}

bool WebCalClient::init()
{
    mClient = iProfile.subProfile("webcal", Buteo::Profile::TYPE_CLIENT);
    if (!mClient) {
        LOG_WARNING("Cannot find client profile.");
        return false;
    }

    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::UTC()));
    mStorage = mKCal::ExtendedCalendar::defaultStorage(mCalendar);
    if (!mStorage || !mStorage->open()) {
        LOG_WARNING("Cannot open default storage.");
        return false;
    }

    // Look for an already existing notebook in storage for this account.
    Q_FOREACH (mKCal::Notebook::Ptr notebook, mStorage->notebooks()) {
        if (notebook->pluginName() == getPluginName() &&
            notebook->syncProfile() == getProfileName()) {
            mNotebookUid = notebook->uid();
            mNotebookEtag = notebook->account().toUtf8(); // Abuse the account to store the etag.
            if (!mStorage->loadNotebookIncidences(mNotebookUid)) {
                LOG_WARNING("Cannot load existing incidences.");
                return false;
            }
            break;
        }
    }
    if (mNotebookUid.isEmpty()) {
        // or create a new one
        mKCal::Notebook::Ptr notebook(new mKCal::Notebook(mClient->key("label"), QString()));
        notebook->setPluginName(getPluginName());
        notebook->setSyncProfile(getProfileName());
        if (!mStorage->addNotebook(notebook)) {
            LOG_WARNING("Cannot create a new notebook." << notebook->name());
            return false;
        }
        mNotebookUid = notebook->uid();
    }
    LOG_DEBUG("Using notebook" << mNotebookUid);

    return true;
}

bool WebCalClient::uninit()
{
    LOG_DEBUG("Closing storage.");
    if (mStorage) {
        mStorage->close();
    }
    mCalendar->close();

    return true;
}

bool WebCalClient::startSync()
{
    QNetworkRequest request(mClient->key("remoteCalendar"));
    if (!mNotebookEtag.isEmpty()) {
        request.setRawHeader("If-None-Match", mNotebookEtag);
    }
    LOG_DEBUG("Requesting" << request.url() << mNotebookEtag);

    QNetworkReply *reply = mNAManager->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(requestFinished()));
    
    return true;
}

void WebCalClient::abortSync(Sync::SyncStatus aStatus)
{
    Q_UNUSED(aStatus);

    mResults = Buteo::SyncResults(QDateTime::currentDateTime().toUTC(),
                                  Buteo::SyncResults::SYNC_RESULT_FAILED,
                                  Buteo::SyncResults::ABORTED);
    emit error(iProfile.name(), QStringLiteral("Synchronization aborted"),
               Buteo::SyncResults::ABORTED);
}

Buteo::SyncResults WebCalClient::getSyncResults() const
{
    return mResults;
}

bool WebCalClient::cleanUp()
{
    if (mNotebookUid.isEmpty()) {
        init();
    }
    LOG_DEBUG("Deleting notebook" << mNotebookUid);
    mKCal::Notebook::Ptr notebook = mStorage->notebook(mNotebookUid);
    if (notebook) {
        return mStorage->deleteNotebook(notebook);
    }
    return true;
}

void WebCalClient::connectivityStateChanged(Sync::ConnectivityType aType, bool aState)
{
    if (aType == Sync::CONNECTIVITY_INTERNET && !aState) {
        // we lost connectivity during sync.
        abortSync(Sync::SYNC_CONNECTION_ERROR);
    }
}

bool WebCalClient::storeCalendar(const QByteArray &icsData, QString &message)
{
    // Start by deleting all previous data.
    KCalCore::Incidence::List localIncidences;
    if (!mStorage->allIncidences(&localIncidences, mNotebookUid)) {
        message = QStringLiteral("Cannot list existing incidences.");
        return false;
    }
    LOG_DEBUG("Deleting" << localIncidences.count() << "previous incidences.");
    for (KCalCore::Incidence::List::ConstIterator it = localIncidences.constBegin();
         it != localIncidences.constEnd(); ++it) {
        if (!mCalendar->deleteIncidence(*it)) {
            message = QStringLiteral("Cannot delete incidence.");
            return false;
        }
    }

    // Recreate all incidences from incoming ICS data.
    mCalendar->addNotebook(mNotebookUid, true);
    mCalendar->setDefaultNotebook(mNotebookUid);
    KCalCore::ICalFormat iCalFormat;
    if (!iCalFormat.fromString(mCalendar, icsData)) {
        message = QStringLiteral("Cannot parse incoming ICS data.");
        return false;
    }
    LOG_DEBUG("Adding" << mCalendar->eventCount() << "new incidences.");
    
    if (!mStorage->save()) {
        message = QStringLiteral("Cannot store data.");
        return false;
    }

    return true;
}

void WebCalClient::requestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        mResults = Buteo::SyncResults(QDateTime::currentDateTime().toUTC(),
                                      Buteo::SyncResults::SYNC_RESULT_FAILED,
                                      Buteo::SyncResults::INTERNAL_ERROR);
        emit error(iProfile.name(), QStringLiteral("No reply object"),
                   Buteo::SyncResults::INTERNAL_ERROR);
        return;
    }

    QByteArray etag;
    Q_FOREACH (const QNetworkReply::RawHeaderPair &header, reply->rawHeaderPairs()) {
        if (header.first.toLower() == QStringLiteral("etag")) {
            etag = header.second;
        }
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (!data.isEmpty()) {
        LOG_DEBUG("Got data with etag" << etag);
        QString message;
        if (storeCalendar(data, message)) {
            mKCal::Notebook::Ptr notebook = mStorage->notebook(mNotebookUid);
            if (notebook) {
                // Ensure that settings for the notebook are consistent.
                notebook->setName(mClient->key("Label"));
                notebook->setAccount(etag);
                notebook->setIsReadOnly(true);
                notebook->setIsMaster(false);
                notebook->setSyncDate(KDateTime::currentUtcDateTime());
                mStorage->updateNotebook(notebook);
            }
        } else {
            mResults = Buteo::SyncResults(QDateTime::currentDateTime().toUTC(),
                                          Buteo::SyncResults::SYNC_RESULT_FAILED,
                                          Buteo::SyncResults::DATABASE_FAILURE);
            emit error(iProfile.name(), message,
                       Buteo::SyncResults::DATABASE_FAILURE);
            return;
        }
    }
    mResults = Buteo::SyncResults(QDateTime::currentDateTime().toUTC(),
                                  Buteo::SyncResults::SYNC_RESULT_SUCCESS,
                                  Buteo::SyncResults::NO_ERROR);
    emit success(iProfile.name(), QStringLiteral("Remote calendar updated successfully."));
}
