/* -*- c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2019 Caliste Damien.
 * Contact: Damien Caliste <dcaliste@free.fr>
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
 */

#include <QtTest>
#include <QObject>
#include <QThread>

#include <webcalclient.h>

class tst_WebCalClient : public QObject
{
    Q_OBJECT

public:
    tst_WebCalClient();
    virtual ~tst_WebCalClient();

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void initCreateWithLabel();
    void initCreateEmpty();
    void initReuse();
    void firstDownload();
    void downloadWithSameEtag();
    void downloadWithDifferentEtag();
    void downloadWithMetaDataUpdateOnly();
    void downloadWithoutEtag();

private:
    void validate();
    void validateSecond();
    void validateThird();

    WebCalClient *mClient;
    QString mNotebookUid;
};

tst_WebCalClient::tst_WebCalClient()
{
}

tst_WebCalClient::~tst_WebCalClient()
{
}

void tst_WebCalClient::initTestCase()
{
    qputenv("SQLITESTORAGEDB", "./db");
    qputenv("MSYNCD_LOGGING_LEVEL", "8");

    QFile::remove("./db");
}

void tst_WebCalClient::cleanupTestCase()
{
}

void tst_WebCalClient::init()
{
    Buteo::SyncProfile webcal(QStringLiteral("webcal-subscription"));
    webcal.merge(Buteo::Profile(QStringLiteral("webcal"), Buteo::Profile::TYPE_CLIENT));
    mClient = new WebCalClient(QStringLiteral("webcal"), webcal, 0);
}

void tst_WebCalClient::cleanup()
{
    delete(mClient);
}

void tst_WebCalClient::initCreateEmpty()
{
    QVERIFY(mClient->init());
    QVERIFY(!mClient->mNotebookUid.isEmpty());
    mNotebookUid = mClient->mNotebookUid;
    QVERIFY(mClient->mNotebookEtag.isEmpty());

    QVERIFY(mClient->mStorage);
    mKCal::Notebook::Ptr notebook = mClient->mStorage->notebook(mNotebookUid);
    QVERIFY(notebook);
    QVERIFY(notebook->name().isEmpty());
    QVERIFY(notebook->description().isEmpty());
}

void tst_WebCalClient::initCreateWithLabel()
{
    Buteo::Profile *client = mClient->profile().clientProfile();
    QVERIFY(client);
    client->setKey(QStringLiteral("label"), QStringLiteral("Web calendar"));

    QVERIFY(mClient->init());
    QVERIFY(!mClient->mNotebookUid.isEmpty());
    mNotebookUid = mClient->mNotebookUid;
    QVERIFY(mClient->mNotebookEtag.isEmpty());

    QVERIFY(mClient->mStorage);
    mKCal::Notebook::Ptr notebook = mClient->mStorage->notebook(mNotebookUid);
    QVERIFY(notebook);
    QCOMPARE(notebook->name(), client->key(QStringLiteral("label")));
    QVERIFY(notebook->description().isEmpty());

    QVERIFY(mClient->mStorage->deleteNotebook(notebook));
    mNotebookUid.clear();
}

void tst_WebCalClient::initReuse()
{
    QVERIFY(mClient->init());
    QCOMPARE(mClient->mNotebookUid, mNotebookUid);
    QVERIFY(mClient->mNotebookEtag.isEmpty());
}

static const QByteArray icsDataFirst(
"BEGIN:VCALENDAR\n"
"METHOD:PUBLISH\n"
"PRODID:-//education.gouv.fr//NONSGML iCalcreator 2.6//\n"
"VERSION:2.0\n"
"X-WR-CALNAME:Calendrier Scolaire - Zone A\n"
"X-WR-CALDESC:education.gouv.fr\n"
"X-WR-TIMEZONE:Europe/Paris\n"
"BEGIN:VEVENT\n"
"UID:608@education.gouv.fr\n"
"DTSTAMP:20190820T144029Z\n"
"DESCRIPTION:Prérentrée des enseignants\n"
"DTSTART;VALUE=DATE:20190830\n"
"LOCATION:Besançon\\, Bordeaux\\, Clermont-Ferrand\\, Dijon\\, Grenoble\\, Limog\n"
" es\\, Lyon\\, Poitiers\n"
"SUMMARY:Prérentrée des enseignants - Zone A\n"
"TRANSP:TRANSPARENT\n"
"END:VEVENT\n"
"END:VCALENDAR\n");
void tst_WebCalClient::validate()
{
    QVERIFY(mClient->mStorage);
    mKCal::Notebook::Ptr notebook = mClient->mStorage->notebook(mNotebookUid);
    QVERIFY(notebook);
    QVERIFY(notebook->isReadOnly());
    QVERIFY(!notebook->isMaster());
    QCOMPARE(notebook->name(), QStringLiteral("Calendrier Scolaire - Zone A"));
    QCOMPARE(notebook->description(), QStringLiteral("education.gouv.fr"));
    QCOMPARE(notebook->customProperty("etag"), QStringLiteral("\"etag\""));

    mKCal::ExtendedCalendar::Ptr cal(new mKCal::ExtendedCalendar(KDateTime::Spec::UTC()));
    mKCal::ExtendedStorage::Ptr store = mKCal::ExtendedCalendar::defaultStorage(cal);
    QVERIFY(store && store->open());
    QVERIFY(store->loadNotebookIncidences(mNotebookUid));
    KCalCore::Incidence::List incidences = cal->incidences();
    QCOMPARE(incidences.count(), 1);
    KCalCore::Incidence::Ptr ev = incidences.first();
    QVERIFY(ev);
    QCOMPARE(ev->uid(), QStringLiteral("608@education.gouv.fr"));
    QCOMPARE(ev->summary(), QStringLiteral("Prérentrée des enseignants - Zone A"));
}

void tst_WebCalClient::firstDownload()
{
    QVERIFY(mClient->init());
    mClient->processData(icsDataFirst, "\"etag\"");

    const Buteo::SyncResults res(mClient->getSyncResults());
    QCOMPARE(res.majorCode(), int(Buteo::SyncResults::SYNC_RESULT_SUCCESS));
    QCOMPARE(res.targetResults().count(), 1);
    Buteo::ItemCounts counts(res.targetResults().first().localItems());
    QCOMPARE(counts.added, unsigned(1));
    QCOMPARE(counts.deleted, unsigned(0));
    QCOMPARE(counts.modified, unsigned(0));

    validate();
}
void tst_WebCalClient::downloadWithSameEtag()
{
    QVERIFY(mClient->init());
    mClient->processData(icsDataFirst, "\"etag\"");

    const Buteo::SyncResults res(mClient->getSyncResults());
    QCOMPARE(res.majorCode(), int(Buteo::SyncResults::SYNC_RESULT_SUCCESS));
    QCOMPARE(res.targetResults().count(), 0);

    validate();
}

static const QByteArray icsDataSecond(
"BEGIN:VCALENDAR\n"
"METHOD:PUBLISH\n"
"PRODID:-//education.gouv.fr//NONSGML iCalcreator 2.6//\n"
"VERSION:2.0\n"
"X-WR-CALNAME:Calendrier Scolaire - Zone B\n"
"X-WR-CALDESC:education.gouv.fr\n"
"X-WR-TIMEZONE:Europe/Paris\n"
"BEGIN:VEVENT\n"
"UID:608@education.gouv.fr\n"
"DTSTAMP:20190820T144029Z\n"
"DESCRIPTION:Prérentrée des enseignants\n"
"DTSTART;VALUE=DATE:20190830\n"
"LOCATION:Besançon\\, Bordeaux\\, Clermont-Ferrand\\, Dijon\\, Grenoble\\, Limog\n"
" es\\, Lyon\\, Poitiers\n"
"SUMMARY:Prérentrée des enseignants - Zone B\n"
"TRANSP:TRANSPARENT\n"
"END:VEVENT\n"
"BEGIN:VEVENT\n"
"UID:609@education.gouv.fr\n"
"DTSTAMP:20190820T144029Z\n"
"DESCRIPTION:Rentrée scolaire des élèves\n"
"DTSTART;VALUE=DATE:20190830\n"
"LOCATION:Besançon\\, Bordeaux\\, Clermont-Ferrand\\, Dijon\\, Grenoble\\, Limog\n"
" es\\, Lyon\\, Poitiers\n"
"SUMMARY:Rentrée scolaire des élèves - Zone B\n"
"TRANSP:TRANSPARENT\n"
"END:VEVENT\n"
"END:VCALENDAR\n");
void tst_WebCalClient::validateSecond()
{
    QVERIFY(mClient->mStorage);
    mKCal::Notebook::Ptr notebook = mClient->mStorage->notebook(mNotebookUid);
    QVERIFY(notebook);
    QVERIFY(notebook->isReadOnly());
    QVERIFY(!notebook->isMaster());
    QCOMPARE(notebook->name(), QStringLiteral("Calendrier Scolaire - Zone B"));
    QCOMPARE(notebook->description(), QStringLiteral("education.gouv.fr"));
    QCOMPARE(notebook->customProperty("etag"), QStringLiteral("\"etag2\""));

    mKCal::ExtendedCalendar::Ptr cal(new mKCal::ExtendedCalendar(KDateTime::Spec::UTC()));
    mKCal::ExtendedStorage::Ptr store = mKCal::ExtendedCalendar::defaultStorage(cal);
    QVERIFY(store && store->open());
    QVERIFY(store->loadNotebookIncidences(mNotebookUid));
    KCalCore::Incidence::List incidences = cal->incidences();
    QCOMPARE(incidences.count(), 2);
    KCalCore::Incidence::Ptr ev1 = cal->incidence(QStringLiteral("608@education.gouv.fr"));
    QVERIFY(ev1);
    KCalCore::Incidence::Ptr ev2 = cal->incidence(QStringLiteral("609@education.gouv.fr"));
    QVERIFY(ev2);
    QCOMPARE(ev1->summary(), QStringLiteral("Prérentrée des enseignants - Zone B"));
    QCOMPARE(ev2->summary(), QStringLiteral("Rentrée scolaire des élèves - Zone B"));
}

void tst_WebCalClient::downloadWithDifferentEtag()
{
    QVERIFY(mClient->init());
    mClient->processData(icsDataSecond, "\"etag2\"");

    const Buteo::SyncResults res(mClient->getSyncResults());
    QCOMPARE(res.majorCode(), int(Buteo::SyncResults::SYNC_RESULT_SUCCESS));
    QCOMPARE(res.targetResults().count(), 1);
    Buteo::ItemCounts counts(res.targetResults().first().localItems());
    QCOMPARE(counts.added, unsigned(2));
    QCOMPARE(counts.deleted, unsigned(1));
    QCOMPARE(counts.modified, unsigned(0));

    validateSecond();
}

void tst_WebCalClient::downloadWithMetaDataUpdateOnly()
{
    Buteo::Profile *client = mClient->profile().clientProfile();
    QVERIFY(client);
    client->setKey(QStringLiteral("label"), QStringLiteral("Web calendar"));

    QVERIFY(mClient->init());
    mClient->processData(icsDataSecond, "\"etag2\"");

    const Buteo::SyncResults res(mClient->getSyncResults());
    QCOMPARE(res.majorCode(), int(Buteo::SyncResults::SYNC_RESULT_SUCCESS));
    QCOMPARE(res.targetResults().count(), 0);

    QVERIFY(mClient->mStorage);
    mKCal::Notebook::Ptr notebook = mClient->mStorage->notebook(mNotebookUid);
    QVERIFY(notebook);
    QVERIFY(notebook->isReadOnly());
    QVERIFY(!notebook->isMaster());
    QCOMPARE(notebook->name(), QStringLiteral("Web calendar"));
    QCOMPARE(notebook->description(), QStringLiteral("education.gouv.fr"));
    QCOMPARE(notebook->customProperty("etag"), QStringLiteral("\"etag2\""));
}

static const QByteArray icsDataThird(
"BEGIN:VCALENDAR\n"
"METHOD:PUBLISH\n"
"PRODID:-//education.gouv.fr//NONSGML iCalcreator 2.6//\n"
"VERSION:2.0\n"
"X-WR-CALNAME:Calendrier Scolaire - Zone C\n"
"X-WR-CALDESC:education.gouv.fr\n"
"X-WR-TIMEZONE:Europe/Paris\n"
"BEGIN:VEVENT\n"
"UID:609@education.gouv.fr\n"
"DTSTAMP:20190820T144029Z\n"
"DESCRIPTION:Rentrée scolaire des élèves\n"
"DTSTART;VALUE=DATE:20190830\n"
"LOCATION:Besançon\\, Bordeaux\\, Clermont-Ferrand\\, Dijon\\, Grenoble\\, Limog\n"
" es\\, Lyon\\, Poitiers\n"
"SUMMARY:Rentrée scolaire des élèves - Zone C\n"
"TRANSP:TRANSPARENT\n"
"END:VEVENT\n"
"END:VCALENDAR\n");
void tst_WebCalClient::validateThird()
{
    QVERIFY(mClient->mStorage);
    mKCal::Notebook::Ptr notebook = mClient->mStorage->notebook(mNotebookUid);
    QVERIFY(notebook);
    QVERIFY(notebook->isReadOnly());
    QVERIFY(!notebook->isMaster());
    QCOMPARE(notebook->name(), QStringLiteral("Calendrier Scolaire - Zone C"));
    QCOMPARE(notebook->description(), QStringLiteral("education.gouv.fr"));
    QVERIFY(notebook->customProperty("etag").isEmpty());

    mKCal::ExtendedCalendar::Ptr cal(new mKCal::ExtendedCalendar(KDateTime::Spec::UTC()));
    mKCal::ExtendedStorage::Ptr store = mKCal::ExtendedCalendar::defaultStorage(cal);
    QVERIFY(store && store->open());
    QVERIFY(store->loadNotebookIncidences(mNotebookUid));
    KCalCore::Incidence::List incidences = cal->incidences();
    QCOMPARE(incidences.count(), 1);
    KCalCore::Incidence::Ptr ev = incidences.first();
    QVERIFY(ev);
    QCOMPARE(ev->uid(), QStringLiteral("609@education.gouv.fr"));
    QCOMPARE(ev->summary(), QStringLiteral("Rentrée scolaire des élèves - Zone C"));
}

void tst_WebCalClient::downloadWithoutEtag()
{
    QVERIFY(mClient->init());
    mClient->processData(icsDataThird, "");

    const Buteo::SyncResults res(mClient->getSyncResults());
    QCOMPARE(res.majorCode(), int(Buteo::SyncResults::SYNC_RESULT_SUCCESS));
    QCOMPARE(res.targetResults().count(), 1);
    Buteo::ItemCounts counts(res.targetResults().first().localItems());
    QCOMPARE(counts.added, unsigned(1));
    QCOMPARE(counts.deleted, unsigned(2));
    QCOMPARE(counts.modified, unsigned(0));

    validateThird();
}

#include "tst_webcalclient.moc"
QTEST_MAIN(tst_WebCalClient)
