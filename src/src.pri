QT -= gui
QT += network dbus

CONFIG += link_pkgconfig c++11

PKGCONFIG += buteosyncfw5 KF5CalendarCore libmkcal-qt5

INCLUDEPATH += $$PWD

SOURCES += \
        $$PWD/webcalclient.cpp

HEADERS += \
        $$PWD/webcalclient.h

OTHER_FILES += \
        $$PWD/xmls/webcal.xml \
        $$PWD/xmls/webcal-sync.xml

MOC_DIR=$$OUT_PWD/.moc/
OBJECTS_DIR=$$OUT_PWD/.obj/
