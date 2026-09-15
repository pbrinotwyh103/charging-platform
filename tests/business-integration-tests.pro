QT += core gui network sql concurrent testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = business_integration_tests
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)
INCLUDEPATH += ../server
HEADERS += business_integration_test.h \
    ../server/app/serverapplication.h \
    ../server/database/databasemanager.h \
    ../server/dispatch/messagedispatcher.h \
    ../server/jobs/jobmanager.h \
    ../server/network/clientsession.h \
    ../server/network/tcpserver.h \
    ../server/services/serviceregistry.h
SOURCES += business_integration_test.cpp \
    ../server/app/serverapplication.cpp \
    ../server/database/databasemanager.cpp \
    ../server/dispatch/messagedispatcher.cpp \
    ../server/jobs/jobmanager.cpp \
    ../server/network/clientsession.cpp \
    ../server/network/tcpserver.cpp \
    ../server/repositories/alarmrepository.cpp \
    ../server/repositories/adminrepository.cpp \
    ../server/repositories/controlrecordrepository.cpp \
    ../server/repositories/favoriterepository.cpp \
    ../server/repositories/orderrepository.cpp \
    ../server/repositories/pilerepository.cpp \
    ../server/repositories/pushrecordrepository.cpp \
    ../server/repositories/reservationrepository.cpp \
    ../server/repositories/stationrepository.cpp \
    ../server/repositories/userrepository.cpp \
    ../server/repositories/walletrepository.cpp \
    ../server/security/passwordhasher.cpp \
    ../server/services/adminservice.cpp \
    ../server/services/alarmservice.cpp \
    ../server/services/analyticsservice.cpp \
    ../server/services/authservice.cpp \
    ../server/services/billingservice.cpp \
    ../server/services/chargingservice.cpp \
    ../server/services/customerserviceservice.cpp \
    ../server/services/orderservice.cpp \
    ../server/services/pileservice.cpp \
    ../server/services/reservationservice.cpp \
    ../server/services/serviceregistry.cpp \
    ../server/services/stationservice.cpp \
    ../server/services/statisticsservice.cpp \
    ../server/services/userservice.cpp
RESOURCES += ../server/resources/database.qrc
