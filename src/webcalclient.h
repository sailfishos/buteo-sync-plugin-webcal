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

#ifndef WEBCALCLIENT_H
#define WEBCALCLIENT_H

#include <ClientPlugin.h>
#include <SyncResults.h>
#include <SyncCommonDefs.h>

#include <extendedstorage.h>

#include <QObject>

#if defined(BUTEOWEBCALPLUGIN_LIBRARY)
#  define SHARED_EXPORT Q_DECL_EXPORT
#else
#  define SHARED_EXPORT Q_DECL_IMPORT
#endif

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
    void requestFinished();

private:
    bool storeCalendar(const QByteArray &icsData, QString &message,
                       unsigned int *added, unsigned int *deleted);

    const Buteo::Profile        *mClient;
    QString                      mNotebookUid;
    QByteArray                   mNotebookEtag;
    mKCal::ExtendedCalendar::Ptr mCalendar;
    mKCal::ExtendedStorage::Ptr  mStorage;

    Buteo::SyncResults          mResults;
};

/*! \brief Creates WebCal client plugin
 *
 * @param aPluginName Name of this client plugin
 * @param aProfile Profile to use
 * @param aCbInterface Pointer to the callback interface
 * @return Client plugin on success, otherwise NULL
 */
extern "C" WebCalClient* createPlugin(const QString &aPluginName,
                                      const Buteo::SyncProfile &aProfile,
                                      Buteo::PluginCbInterface *aCbInterface);

/*! \brief Destroys WebCal client plugin
 *
 * @param aClient WebCal client plugin instance to destroy
 */
extern "C" void destroyPlugin(WebCalClient *aClient);

#endif // WEBCALCLIENT_H
