QT += core sql testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = database_repository_tests
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += ../server

HEADERS += \
    database_repository_test.h \
    ../server/database/databasemanager.h \
    ../server/repositories/alarmrepository.h \
    ../server/repositories/controlrecordrepository.h \
    ../server/repositories/favoriterepository.h \
    ../server/repositories/orderrepository.h \
    ../server/repositories/pilerepository.h \
    ../server/repositories/pushrecordrepository.h \
    ../server/repositories/repositorybase.h \
    ../server/repositories/reservationrepository.h \
    ../server/repositories/stationrepository.h \
    ../server/repositories/userrepository.h \
    ../server/repositories/walletrepository.h

SOURCES += \
    database_repository_test.cpp \
    ../server/database/databasemanager.cpp \
    ../server/repositories/alarmrepository.cpp \
    ../server/repositories/controlrecordrepository.cpp \
    ../server/repositories/favoriterepository.cpp \
    ../server/repositories/orderrepository.cpp \
    ../server/repositories/pilerepository.cpp \
    ../server/repositories/pushrecordrepository.cpp \
    ../server/repositories/reservationrepository.cpp \
    ../server/repositories/stationrepository.cpp \
    ../server/repositories/userrepository.cpp \
    ../server/repositories/walletrepository.cpp

RESOURCES += ../server/resources/database.qrc
