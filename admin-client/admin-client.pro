QT += core gui widgets network charts
CONFIG += c++17
TEMPLATE = app
TARGET = charging_admin_client
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)

HEADERS += \
    charts/pilestatuschartwidget.h \
    charts/revenuechartwidget.h \
    controllers/admincontroller.h \
    pages/alarmspage.h \
    pages/assetspage.h \
    pages/monitorpage.h \
    pages/overviewpage.h \
    pages/recordspage.h \
    widgets/adminuihelpers.h \
    widgets/paginationbar.h \
    ui/adminmainwindow.h

SOURCES += \
    main.cpp \
    charts/pilestatuschartwidget.cpp \
    charts/revenuechartwidget.cpp \
    controllers/admincontroller.cpp \
    pages/alarmspage.cpp \
    pages/assetspage.cpp \
    pages/monitorpage.cpp \
    pages/overviewpage.cpp \
    pages/recordspage.cpp \
    widgets/paginationbar.cpp \
    ui/adminmainwindow.cpp
