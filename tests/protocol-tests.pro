QT += core network testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = protocol_tests
DESTDIR = $$OUT_PWD/../bin

include(../common/common.pri)

SOURCES += protocol_test.cpp
