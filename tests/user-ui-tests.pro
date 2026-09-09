QT += core gui widgets network webenginewidgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = user_ui_tests
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += ../common ../user-client

HEADERS += \
    user-client/userui_test.h \
    ../user-client/map/mapnavigator.h \
    ../user-client/pages/chargingpage.h \
    ../user-client/pages/customerservicepage.h \
    ../user-client/pages/homepage.h \
    ../user-client/pages/profilepage.h \
    ../user-client/pages/stationdetailpage.h \
    ../user-client/pages/stationutils.h \
    ../user-client/ui/usermainwindow.h

SOURCES += \
    user-client/userui_test.cpp \
    ../user-client/map/mapnavigator.cpp \
    ../user-client/pages/chargingpage.cpp \
    ../user-client/pages/customerservicepage.cpp \
    ../user-client/pages/homepage.cpp \
    ../user-client/pages/profilepage.cpp \
    ../user-client/pages/stationdetailpage.cpp \
    ../user-client/pages/stationutils.cpp \
    ../user-client/ui/usermainwindow.cpp
