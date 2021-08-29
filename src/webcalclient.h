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

#ifndef WEBCALCLIENT_H
#define WEBCALCLIENT_H

#include <ClientPlugin.h>
#include <SyncResults.h>
#include <SyncCommonDefs.h>
#include <SyncPluginLoader.h>

#include <extendedstorage.h>

#include <QObject>
#include <QLoggingCategory>

#if defined(BUTEOWEBCALPLUGIN_LIBRARY)
#  define SHARED_EXPORT Q_DECL_EXPORT
#else
#  define SHARED_EXPORT Q_DECL_IMPORT
#endif

class QNetworkReply;

class SHARED_EXPORT WebCalClient : public Buteo::ClientPlugin
{
    Q_OBJECT

public:
    WebCalClient(const QString &aPluginName,
                 const Buteo::SyncProfile &aProfile,
                 Buteo::PluginCbInterface *aCbInterface);
    virtual ~WebCalClient();

    virtual bool init();
    virtual bool uninit();
    virtual bool startSync();
    virtual void abortSync(Sync::SyncStatus aStatus = Sync::SYNC_ABORTED);
    virtual Buteo::SyncResults getSyncResults() const;
    virtual bool cleanUp();

public Q_SLOTS:
    virtual void connectivityStateChanged(Sync::ConnectivityType aType, bool aState);

private Q_SLOTS:
    void dataReceived();

private:
    void succeed(const QString &label, unsigned int added, unsigned int deleted);
    void failed(Buteo::SyncResults::MinorCode code, const QString &message);
    void processData(const QByteArray &icsData, const QByteArray &etag);

    const Buteo::Profile        *mClient;
    QString                      mNotebookUid;
    QByteArray                   mNotebookEtag;
    mKCal::ExtendedCalendar::Ptr mCalendar;
    mKCal::ExtendedStorage::Ptr  mStorage;

    QNetworkReply               *mReply;
    Buteo::SyncResults           mResults;

    friend class tst_WebCalClient;
};

class WebCalClientLoader : public Buteo::SyncPluginLoader
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.sailfishos.plugins.sync.WebCalClientLoader")
    Q_INTERFACES(Buteo::SyncPluginLoader)

public:
    /*! \brief Creates WebCal client plugin
     *
     * @param aPluginName Name of this client plugin
     * @param aProfile Profile to use
     * @param aCbInterface Pointer to the callback interface
     * @return Client plugin on success, otherwise NULL
     */
    Buteo::ClientPlugin* createClientPlugin(const QString& pluginName,
                                            const Buteo::SyncProfile& profile,
                                            Buteo::PluginCbInterface* cbInterface) override;
};

#endif // WEBCALCLIENT_H
