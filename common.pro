

SOURCES +=  ../src/packet.c \
            ../src/crc8.c \
            ../src/io.c \
            ../src/timer.c \
            ../src/utils.c \
            ../src/find_next_bit.c \
            ../src/bitmap.c \
            ../src/hweight.c


HEADERS +=  ../src/protocol.h \
            ../src/packet.h \
            ../src/io.h \
            ../src/common.h \
            ../src/list.h \
            ../src/utils.h \
            ../src/bitmap.h \
            ../src/bits.h \
            ../src/hweight.h \
            ../src/timer.h

linux-arm-g++ {
    SOURCES += ../src/findbit.S
}
