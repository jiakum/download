
QT      =


TARGET = download
TEMPLATE = app

SOURCES +=  ../src/packet.c \
            ../src/crc8.c \
            ../src/io.c \
            server.c \
            ../src/timer.c \
            ../src/utils.c \
            main.c


HEADERS +=  ../src/protocol.h \
            ../src/packet.h \
            ../src/io.h \
            ../src/common.h \
            ../src/list.h \
            ../src/utils.h \
            ../src/timer.h

INCLUDEPATH += ../src/
