QT += core network
CONFIG += c++17

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/app/appinfo.h \
    $$PWD/models/role.h \
    $$PWD/protocol/errorcodes.h \
    $$PWD/protocol/message.h \
    $$PWD/protocol/messagetypes.h \
    $$PWD/protocol/packetcodec.h \
    $$PWD/network/clientconnection.h

SOURCES += \
    $$PWD/protocol/packetcodec.cpp \
    $$PWD/network/clientconnection.cpp
