QT -= gui
QT += network dbus

CONFIG += link_pkgconfig console

PKGCONFIG += buteosyncfw5 libkcalcoren-qt5 libmkcal-qt5

INCLUDEPATH += $$PWD

SOURCES += \
        $$PWD/webcalclient.cpp

HEADERS += \
        $$PWD/webcalclient.h

OTHER_FILES += \
        $$PWD/xmls/webcal.xml \
        $$PWD/xmls/webcal-sync.xml

MOC_DIR=$$PWD/.moc/
OBJECTS_DIR=$$PWD/.obj/
