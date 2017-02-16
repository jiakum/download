
QT      =


TARGET = download
TEMPLATE = app

SOURCES +=  packet.c \
            crc8.c \
            io.c \
            main.c


HEADERS +=  protocol.h \
            packet.h \
            io.h
