QT += core gui network sql concurrent testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = phase1_tests
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)
INCLUDEPATH += ../server

HEADERS += \
    phase1_test.h \
    ../server/app/serverapplication.h \
    ../server/database/databasemanager.h \
    ../server/dispatch/messagedispatcher.h \
    ../server/jobs/jobmanager.h \
    ../server/network/clientsession.h \
    ../server/network/tcpserver.h \
    ../server/repositories/adminrepository.h \
    ../server/repositories/repositorybase.h \
    ../server/repositories/userrepository.h \
    ../server/security/passwordhasher.h \
    ../server/services/authservice.h \
    ../server/services/serviceregistry.h

SOURCES += \
    phase1_test.cpp \
    ../server/app/serverapplication.cpp \
    ../server/database/databasemanager.cpp \
    ../server/dispatch/messagedispatcher.cpp \
    ../server/jobs/jobmanager.cpp \
    ../server/network/clientsession.cpp \
    ../server/network/tcpserver.cpp \
    ../server/repositories/adminrepository.cpp \
    ../server/repositories/userrepository.cpp \
    ../server/security/passwordhasher.cpp \
    ../server/services/authservice.cpp \
    ../server/services/serviceregistry.cpp

RESOURCES += ../server/resources/database.qrc

# ServiceRegistry and MessageDispatcher now link the complete business core.
# Keep this list explicit: qmake can de-duplicate wildcard entries that were
# removed from SOURCES earlier, leaving an incomplete link on a clean build.
SOURCES += \
    ../server/repositories/alarmrepository.cpp \
    ../server/repositories/controlrecordrepository.cpp \
    ../server/repositories/favoriterepository.cpp \
    ../server/repositories/orderrepository.cpp \
    ../server/repositories/pilerepository.cpp \
    ../server/repositories/pushrecordrepository.cpp \
    ../server/repositories/reservationrepository.cpp \
    ../server/repositories/stationrepository.cpp \
    ../server/repositories/walletrepository.cpp \
    ../server/services/adminservice.cpp \
    ../server/services/alarmservice.cpp \
    ../server/services/analyticsservice.cpp \
    ../server/services/billingservice.cpp \
    ../server/services/chargingservice.cpp \
    ../server/services/customerserviceservice.cpp \
    ../server/services/decisionservice.cpp \
    ../server/services/orderservice.cpp \
    ../server/services/pileservice.cpp \
    ../server/services/reservationservice.cpp \
    ../server/services/stationservice.cpp \
    ../server/services/statisticsservice.cpp \
    ../server/services/userservice.cpp
