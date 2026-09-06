QT += core gui widgets network webenginewidgets
CONFIG += c++17
TEMPLATE = app
TARGET = charging_user_client
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)

HEADERS += \
    api/clientapi.h \
    stores/snapshotstore.h \
    controllers/usercontroller.h \
    ui/usermainwindow.h \
    pages/homepage.h \
    pages/chargingpage.h \
    pages/profilepage.h \
    pages/stationutils.h \
    pages/stationdetailpage.h
    map/mapnavigator.h

SOURCES += \
    main.cpp \
    api/clientapi.cpp \
    stores/snapshotstore.cpp \
    controllers/usercontroller.cpp \
    ui/usermainwindow.cpp \
     pages/homepage.cpp \
    pages/chargingpage.cpp \
    pages/profilepage.cpp \
    pages/stationutils.cpp \
    pages/stationdetailpage.cpp
    map/mapnavigator.cpp
