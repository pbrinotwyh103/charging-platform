QT += core gui widgets charts testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = admin_ui_tests
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += ../common ../admin-client

HEADERS += \
    admin-client/adminui_test.h \
    ../admin-client/charts/pilestatuschartwidget.h \
    ../admin-client/charts/revenuechartwidget.h \
    ../admin-client/pages/alarmspage.h \
    ../admin-client/pages/assetspage.h \
    ../admin-client/pages/monitorpage.h \
    ../admin-client/pages/overviewpage.h \
    ../admin-client/pages/recordspage.h \
    ../admin-client/ui/adminmainwindow.h \
    ../admin-client/widgets/adminuihelpers.h \
    ../admin-client/widgets/paginationbar.h

SOURCES += \
    admin-client/adminui_test.cpp \
    ../admin-client/charts/pilestatuschartwidget.cpp \
    ../admin-client/charts/revenuechartwidget.cpp \
    ../admin-client/pages/alarmspage.cpp \
    ../admin-client/pages/assetspage.cpp \
    ../admin-client/pages/monitorpage.cpp \
    ../admin-client/pages/overviewpage.cpp \
    ../admin-client/pages/recordspage.cpp \
    ../admin-client/ui/adminmainwindow.cpp \
    ../admin-client/widgets/paginationbar.cpp
