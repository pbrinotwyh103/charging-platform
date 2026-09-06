QT += core gui widgets testlib
QMAKE_CC = gcc-14
QMAKE_CXX = g++-14
QMAKE_LINK = g++-14
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = user_client_tests
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += ../user-client

HEADERS += \
    ../user-client/pages/homepage.h \
    ../user-client/pages/chargingpage.h \
    ../user-client/pages/stationdetailpage.h \
    ../user-client/pages/profilepage.h \
    ../user-client/widgets/nicknamedialog.h \
    ../user-client/widgets/rechargedialog.h \
    ../user-client/pages/stationutils.h \
    ../user-client/stores/snapshotstore.h \
    ../user-client/map/mapnavigator.h

SOURCES += \
    homepage_test.cpp \
    ../user-client/pages/homepage.cpp \
    ../user-client/pages/chargingpage.cpp \
    ../user-client/pages/stationdetailpage.cpp \
    ../user-client/pages/profilepage.cpp \
    ../user-client/widgets/nicknamedialog.cpp \
    ../user-client/widgets/rechargedialog.cpp \
    ../user-client/pages/stationutils.cpp \
    ../user-client/stores/snapshotstore.cpp \
    ../user-client/map/mapnavigator.cpp
