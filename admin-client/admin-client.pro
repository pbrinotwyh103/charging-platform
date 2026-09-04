QT += core gui widgets network charts
CONFIG += c++17
TEMPLATE = app
TARGET = charging_admin_client
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)

HEADERS += \
    controllers/admincontroller.h \
    ui/adminmainwindow.h

SOURCES += \
    main.cpp \
    controllers/admincontroller.cpp \
    ui/adminmainwindow.cpp
