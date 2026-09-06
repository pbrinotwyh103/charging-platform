QT += core gui sql testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = service_tests
DESTDIR = $$OUT_PWD/../bin
INCLUDEPATH += ../server ../common
HEADERS += service_test.h
SOURCES += service_test.cpp \
    ../common/protocol/packetcodec.cpp \
    ../server/services/userservice.cpp \
    ../server/services/stationservice.cpp \
    ../server/services/pileservice.cpp \
    ../server/services/reservationservice.cpp \
    ../server/services/orderservice.cpp \
    ../server/services/billingservice.cpp \
    ../server/services/chargingservice.cpp \
    ../server/services/alarmservice.cpp \
    ../server/services/adminservice.cpp \
    ../server/services/statisticsservice.cpp \
    ../server/repositories/controlrecordrepository.cpp \
    ../server/repositories/alarmrepository.cpp \
    ../server/database/databasemanager.cpp \
    ../server/repositories/userrepository.cpp \
    ../server/repositories/walletrepository.cpp \
    ../server/repositories/stationrepository.cpp \
    ../server/repositories/pilerepository.cpp \
    ../server/repositories/reservationrepository.cpp \
    ../server/repositories/orderrepository.cpp \
    ../server/repositories/favoriterepository.cpp
RESOURCES += ../server/resources/database.qrc
