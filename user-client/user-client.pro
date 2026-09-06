QT += core gui widgets network webenginewidgets
CONFIG += c++17
TEMPLATE = app
TARGET = charging_user_client
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)

HEADERS += \
    api/clientapi.h \
    stores/snapshotstore.h \
    ui/usermainwindow.h \
    pages/homepage.h \
    pages/chargingpage.h \
    pages/favoritespage.h \
    pages/profilepage.h \
    pages/rechargerecordspage.h \
    pages/stationutils.h \
    pages/stationdetailpage.h \
    widgets/nicknamedialog.h \
    widgets/rechargedialog.h \
    map/mapnavigator.h

SOURCES += \
    main.cpp \
    api/clientapi.cpp \
    stores/snapshotstore.cpp \
    ui/usermainwindow.cpp \
     pages/homepage.cpp \
    pages/chargingpage.cpp \
    pages/favoritespage.cpp \
    pages/profilepage.cpp \
    pages/rechargerecordspage.cpp \
    pages/stationutils.cpp \
    pages/stationdetailpage.cpp \
    widgets/nicknamedialog.cpp \
    widgets/rechargedialog.cpp \
    map/mapnavigator.cpp
