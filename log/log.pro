#-------------------------------------------------
#
# Project created by QtCreator 2016-12-08T10:50:13
#
#-------------------------------------------------

QT       = 

TARGET = logd
TEMPLATE = app


SOURCES +=  log.c    \
            logd.c   \
            file.c   \
            socket.c

HEADERS  += ../include/log.h    \
            file.h   \

INCLUDEPATH += ../utils/ ../include/
