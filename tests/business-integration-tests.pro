QT += core gui network sql concurrent testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = business_integration_tests
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)
INCLUDEPATH += ../server
HEADERS += business_integration_test.h \
    $$files($$PWD/../server/app/*.h) \
    $$files($$PWD/../server/database/*.h) \
    $$files($$PWD/../server/dispatch/*.h) \
    $$files($$PWD/../server/jobs/*.h) \
    $$files($$PWD/../server/network/*.h) \
    $$files($$PWD/../server/services/*.h)
SOURCES += business_integration_test.cpp \
    $$files($$PWD/../server/app/*.cpp) \
    $$files($$PWD/../server/database/*.cpp) \
    $$files($$PWD/../server/dispatch/*.cpp) \
    $$files($$PWD/../server/jobs/*.cpp) \
    $$files($$PWD/../server/network/*.cpp) \
    $$files($$PWD/../server/repositories/*.cpp) \
    $$files($$PWD/../server/security/*.cpp) \
    $$files($$PWD/../server/services/*.cpp)
RESOURCES += ../server/resources/database.qrc
