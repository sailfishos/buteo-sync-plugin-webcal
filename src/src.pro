TARGET = webcal-client

include(src.pri)

QMAKE_CXXFLAGS += -Wall \
    -g \
    -Wno-cast-align \
    -O2 -finline-functions

DEFINES += BUTEOWEBDAVPLUGIN_LIBRARY

TEMPLATE = lib
CONFIG += plugin
target.path = $$[QT_INSTALL_LIBS]/buteo-plugins-qt5/oopp

sync.path = /etc/buteo/profiles/sync
sync.files = xmls/webcal-sync.xml

client.path = /etc/buteo/profiles/client
client.files = xmls/webcal.xml

INSTALLS += target sync client
