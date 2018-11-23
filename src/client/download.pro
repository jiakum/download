
QT      =

QMAKE_CFLAGS += -Wno-unused-parameter

TARGET = download
TEMPLATE = app

SOURCES +=  client.c \
            main.c

HEADERS += 

include(../common.pro)

INCLUDEPATH += ../src/ ../include/ ../

LIBS += -lrt
