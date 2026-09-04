QT += core gui widgets network webenginewidgets
CONFIG += c++17
TEMPLATE = app
TARGET = charging_user_client
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)

HEADERS += \
    controllers/usercontroller.h \
    ui/usermainwindow.h

SOURCES += \
    main.cpp \
    controllers/usercontroller.cpp \
    ui/usermainwindow.cpp
