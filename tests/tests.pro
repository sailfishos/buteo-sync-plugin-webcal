TEMPLATE = app
TARGET = tst_webcalclient

QT += testlib
CONFIG += debug

include($$PWD/../src/src.pri)

SOURCES += tst_webcalclient.cpp

target.path = /opt/tests/buteo/plugins/webcal/

INSTALLS += target
