QT += core network sql
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = charging_server
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)

HEADERS += \
    app/serverapplication.h \
    database/databasemanager.h \
    dispatch/messagedispatcher.h \
    jobs/jobmanager.h \
    network/clientsession.h \
    network/tcpserver.h \
    repositories/alarmrepository.h \
    repositories/orderrepository.h \
    repositories/pilerepository.h \
    repositories/repositorybase.h \
    repositories/reservationrepository.h \
    repositories/stationrepository.h \
    repositories/userrepository.h \
    repositories/walletrepository.h \
    services/adminservice.h \
    services/alarmservice.h \
    services/authservice.h \
    services/billingservice.h \
    services/chargingservice.h \
    services/orderservice.h \
    services/pileservice.h \
    services/reservationservice.h \
    services/servicebase.h \
    services/serviceregistry.h \\
    services/stationservice.h \
    services/statisticsservice.h \
    services/userservice.h

SOURCES += \
    main.cpp \
    app/serverapplication.cpp \
    database/databasemanager.cpp \
    dispatch/messagedispatcher.cpp \
    jobs/jobmanager.cpp \
    network/clientsession.cpp \
    network/tcpserver.cpp \
    services/serviceregistry.cpp
