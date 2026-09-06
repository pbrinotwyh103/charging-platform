QT += core network sql concurrent
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
    repositories/adminrepository.h \
    repositories/controlrecordrepository.h \
    repositories/favoriterepository.h \
    repositories/orderrepository.h \
    repositories/pilerepository.h \
    repositories/pushrecordrepository.h \
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
    services/serviceregistry.h \
    services/stationservice.h \
    services/statisticsservice.h \
    services/userservice.h \
    security/passwordhasher.h

SOURCES += \
    main.cpp \
    app/serverapplication.cpp \
    database/databasemanager.cpp \
    dispatch/messagedispatcher.cpp \
    jobs/jobmanager.cpp \
    network/clientsession.cpp \
    network/tcpserver.cpp \
    repositories/alarmrepository.cpp \
    repositories/adminrepository.cpp \
    repositories/controlrecordrepository.cpp \
    repositories/favoriterepository.cpp \
    repositories/orderrepository.cpp \
    repositories/pilerepository.cpp \
    repositories/pushrecordrepository.cpp \
    repositories/reservationrepository.cpp \
    repositories/stationrepository.cpp \
    repositories/userrepository.cpp \
    repositories/walletrepository.cpp \
    security/passwordhasher.cpp \
    services/authservice.cpp \
    services/serviceregistry.cpp

RESOURCES += resources/database.qrc
