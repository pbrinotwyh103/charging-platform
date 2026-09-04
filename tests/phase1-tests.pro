QT += core network sql concurrent testlib
QT -= gui
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
