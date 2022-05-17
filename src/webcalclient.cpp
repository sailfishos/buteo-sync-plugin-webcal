/*
 * This file is part of buteo-sync-plugin-webcal package
 *
 * Copyright (C) 2019 Damien Caliste <dcaliste@free.fr>.
 * Copyright (C) 2021 Jolla Ltd.
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

#include <PluginCbInterface.h>

#include <KCalendarCore/ICalFormat>

Q_LOGGING_CATEGORY(lcWebCal, "buteo.plugin.webcal", QtWarningMsg)

Buteo::ClientPlugin* WebCalClientLoader::createClientPlugin(
        const QString& pluginName,
        const Buteo::SyncProfile& profile,
        Buteo::PluginCbInterface* cbInterface)
{
    return new WebCalClient(pluginName, profile, cbInterface);
}


WebCalClient::WebCalClient(const QString& aPluginName,
                           const Buteo::SyncProfile& aProfile,
                           Buteo::PluginCbInterface *aCbInterface)
    : ClientPlugin(aPluginName, aProfile, aCbInterface)
    , mCalendar(nullptr)
    , mStorage(nullptr)
    , mReply(nullptr)
{
}

WebCalClient::~WebCalClient()
{
    delete mReply;
}

static const QByteArray ETAG_PROPERTY("etag");
bool WebCalClient::init()
{
    emit syncProgressDetail(iProfile.name(), Sync::SYNC_PROGRESS_INITIALISING);

    mClient = iProfile.clientProfile();
    if (!mClient) {
        qCWarning(lcWebCal) << "Cannot find client profile.";
        return false;
    }

    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::utc()));
    mStorage = mKCal::ExtendedCalendar::defaultStorage(mCalendar);
    if (!mStorage || !mStorage->open()) {
        qCWarning(lcWebCal) << "Cannot open default storage.";
        return false;
    }

    // Look for an already existing notebook in storage for this sync profile.
    const QList<mKCal::Notebook> notebooks = mStorage->notebooks();
    for (const mKCal::Notebook &notebook : notebooks) {
        if (notebook.pluginName() == getPluginName() &&
            notebook.syncProfile() == getProfileName()) {
            mNotebookUid = notebook.uid();
            mNotebookEtag = notebook.customProperty(ETAG_PROPERTY).toUtf8();
            break;
        }
    }
    if (mNotebookUid.isEmpty()) {
        // or create a new one
        mKCal::Notebook notebook(mClient->key("label"), QString());
        notebook.setPluginName(getPluginName());
        notebook.setSyncProfile(getProfileName());
        notebook.setIsReadOnly(true);
        if (!mStorage->addNotebook(notebook)) {
            qCWarning(lcWebCal) << "Cannot create a new notebook" << notebook.uid();
            return false;
        }
        mNotebookUid = notebook.uid();
    }
    qCDebug(lcWebCal) << "Using notebook" << mNotebookUid;

    return true;
}

bool WebCalClient::uninit()
{
    qCDebug(lcWebCal) << "Closing storage.";
    if (mStorage) {
        mStorage->close();
    }
    mCalendar->close();

    return true;
}

bool WebCalClient::startSync()
{
    QNetworkRequest request(mClient->key("remoteCalendar"));
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute,
                         mClient->boolKey("allowRedirect"));
    if (!mNotebookEtag.isEmpty()) {
        request.setRawHeader("If-None-Match", mNotebookEtag);
    }
    qCDebug(lcWebCal) << "Requesting" << request.url() << mNotebookEtag;

    QNetworkAccessManager *accessManager = new QNetworkAccessManager(this);
    mReply = accessManager->get(request);
    connect(mReply, &QNetworkReply::finished, [this] {
            emit syncProgressDetail(iProfile.name(), Sync::SYNC_PROGRESS_FINALISING);
            mReply->deleteLater();
            if (mReply->error() != QNetworkReply::NoError
                && mReply->error() != QNetworkReply::OperationCanceledError) {
                qCWarning(lcWebCal) << mReply->readAll();
                failed(Buteo::SyncResults::CONNECTION_ERROR,
                       QStringLiteral("Network issue: %1.").arg(mReply->error()));
            } else if (mReply->error() == QNetworkReply::NoError) {
                processData(mReply->readAll(), mReply->rawHeader("etag"));
            }
            mReply = nullptr;
        });
    connect(mReply, &QIODevice::readyRead, this, &WebCalClient::dataReceived);
    
    return true;
}

void WebCalClient::abortSync(Sync::SyncStatus aStatus)
{
    Q_UNUSED(aStatus);

    failed(Buteo::SyncResults::ABORTED, QStringLiteral("Synchronization aborted."));
    if (mReply) {
        mReply->abort();
    }
}

void WebCalClient::succeed(const QString &label, unsigned int added, unsigned int deleted)
{
    mResults = Buteo::SyncResults(QDateTime::currentDateTime().toUTC(),
                                  Buteo::SyncResults::SYNC_RESULT_SUCCESS,
                                  Buteo::SyncResults::NO_ERROR);
    if (added || deleted) {
        mResults.addTargetResults
            (Buteo::TargetResults(label.isEmpty() ? mNotebookUid : label,
                                  Buteo::ItemCounts(added, deleted, 0),
                                  Buteo::ItemCounts()));
    }
    emit success(iProfile.name(), QStringLiteral("Remote calendar updated successfully."));
}

void WebCalClient::failed(Buteo::SyncResults::MinorCode code, const QString &message)
{
    mResults = Buteo::SyncResults(QDateTime::currentDateTime().toUTC(),
                                  Buteo::SyncResults::SYNC_RESULT_FAILED, code);
    emit error(iProfile.name(), message, code);
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
    qCDebug(lcWebCal) << "Deleting notebook" << mNotebookUid;
    return mStorage->deleteNotebook(mNotebookUid);
}

void WebCalClient::connectivityStateChanged(Sync::ConnectivityType aType, bool aState)
{
    if (aType == Sync::CONNECTIVITY_INTERNET && !aState) {
        // we lost connectivity during sync.
        abortSync(Sync::SYNC_CONNECTION_ERROR);
    }
}

void WebCalClient::dataReceived()
{
    emit syncProgressDetail(iProfile.name(), Sync::SYNC_PROGRESS_RECEIVING_ITEMS);
}

void WebCalClient::processData(const QByteArray &icsData, const QByteArray &etag)
{
    mKCal::Notebook notebook = mStorage->notebook(mNotebookUid);
    if (!notebook.isValid()) {
        failed(Buteo::SyncResults::DATABASE_FAILURE,
               QStringLiteral("Cannot find notebook."));
        return;
    }

    unsigned int added = 0, deleted = 0;
    qCDebug(lcWebCal) << "Got etag" << etag << "was" << mNotebookEtag;
    if (etag.isEmpty() || etag != mNotebookEtag) {
        // Start by deleting all previous data.
        if (!mStorage->loadNotebookIncidences(mNotebookUid)) {
            failed(Buteo::SyncResults::DATABASE_FAILURE,
                   QStringLiteral("Cannot load existing incidences."));
            return;
        }
        deleted = mCalendar->incidences().count();
        qCDebug(lcWebCal) << "Deleting" << deleted << "previous incidences.";
        mCalendar->deleteAllIncidences();
        // Deletion happens after insertion in mkcal, so ensure
        // that incidences with a UID in icsData are deleted before.
        if (deleted && !mStorage->save(mKCal::ExtendedStorage::PurgeDeleted)) {
            failed(Buteo::SyncResults::DATABASE_FAILURE,
                   QStringLiteral("Cannot delete previous data."));
            return;
        }

        // Recreate all incidences from incoming ICS data.
        mCalendar->addNotebook(mNotebookUid, true);
        mCalendar->setDefaultNotebook(mNotebookUid);
        KCalendarCore::ICalFormat iCalFormat;
        qCDebug(lcWebCal) << icsData;
        if (!icsData.isEmpty() && !iCalFormat.fromRawString(mCalendar, icsData)) {
            failed(Buteo::SyncResults::DATABASE_FAILURE,
                   QStringLiteral("Cannot parse incoming ICS data."));
            return;
        }
        added = mCalendar->incidences().count();
        qCDebug(lcWebCal) << "Adding" << added << "new incidences.";
        qCDebug(lcWebCal) << "From calendar" << mCalendar->nonKDECustomProperty("X-WR-CALNAME")
                  << mCalendar->nonKDECustomProperty("X-WR-CALDESC");
        if (added && !mStorage->save()) {
            failed(Buteo::SyncResults::DATABASE_FAILURE,
                   QStringLiteral("Cannot store data."));
            return;
        }

        // Record the etag so we only update in future if necessary.
        notebook.setCustomProperty(ETAG_PROPERTY, etag);
        // Store calendar name, if auto-detect has been requested.
        if (mClient->key("label").isEmpty()) {
            notebook.setName(mCalendar->nonKDECustomProperty("X-WR-CALNAME"));
        }
        if (!mCalendar->nonKDECustomProperty("X-WR-CALDESC").isEmpty()
            && mCalendar->nonKDECustomProperty("X-WR-CALDESC") != notebook.name()) {
            notebook.setDescription(mCalendar->nonKDECustomProperty("X-WR-CALDESC"));
        }
    }
    // Ensure that settings for the notebook are consistent.
    if (!mClient->key("label").isEmpty()) {
        notebook.setName(mClient->key("label"));
    }
    if (!iProfile.key("accountid").isEmpty()) {
        notebook.setAccount(iProfile.key("accountid"));
    }
    notebook.setIsReadOnly(true);
    notebook.setIsMaster(false);
    notebook.setSyncDate(QDateTime::currentDateTimeUtc());
    if (!mStorage->updateNotebook(notebook)) {
        failed(Buteo::SyncResults::DATABASE_FAILURE,
               QStringLiteral("Cannot update notebook."));
        return;
    }

    succeed(notebook.name(), added, deleted);
}

