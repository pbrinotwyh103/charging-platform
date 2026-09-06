QT += core network testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = admin_controller_tests
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)
INCLUDEPATH += ../admin-client

HEADERS += \
    ../admin-client/controllers/admincontroller.h

SOURCES += \
    admin-client/admincontroller_test.cpp \
    ../admin-client/controllers/admincontroller.cpp
