

SOURCES +=  ../src/packet.c \
            ../src/crc8.c \
            ../src/io.c \
            ../src/timer.c \
            ../src/utils.c \
            ../src/find_next_bit.c \
            ../src/bitmap.c \
            ../src/hweight.c \
            ../log/log.c \
            ../log/socket.c


HEADERS +=  ../src/protocol.h \
            ../src/packet.h \
            ../src/io.h \
            ../include/common.h \
            ../include/list.h \
            ../include/utils.h \
            ../src/bitmap.h \
            ../src/bits.h \
            ../src/hweight.h \
            ../include/log.h \
            ../include/timer.h

linux-arm-g++ {
    SOURCES += ../src/findbit.S
}
