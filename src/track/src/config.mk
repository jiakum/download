####### Compiler, tools and options

SDK_PATH = /home/mjk/project/linux/ipc/ipc_20180518_v5/cbb/lib/X86/basic

CROSS_COMPILE = 
CC            = ${CROSS_COMPILE}gcc
CXX           = ${CROSS_COMPILE}g++
DEFINES       = 
CFLAGS        += -std=gnu++11 -O2 -Wall -Wno-unused-parameter -W $(DEFINES)
CXXFLAGS      += -std=c++11 -O2 -Wall -Wno-unused-parameter -W $(DEFINES)
INCPATH       += -I./include -I. -I$(SDK_PATH)/libtinyxml/include/ -I$(SDK_PATH)/libjson/include/
LINK          = ${CXX}
LFLAGS        += -Wl,-O1 -L$(SDK_PATH)/libjson/ -L$(SDK_PATH)/libtinyxml/
LIBS          += $(SUBLIBS)  -lpthread 
AR            = ${CROSS_COMPILE}ar cqs
RANLIB        = 
TAR           = tar -cf
COMPRESS      = gzip -9f
COPY          = cp -f
SED           = sed
COPY_FILE     = $(COPY)
COPY_DIR      = $(COPY) -r
STRIP         = ${CROSS_COMPILE}strip
INSTALL_FILE  = install -m 644 -p
INSTALL_DIR   = $(COPY_DIR)
INSTALL_PROGRAM = install -m 755 -p
DEL_FILE      = rm -f
SYMLINK       = ln -f -s
DEL_DIR       = rmdir
MOVE          = mv -f
CHK_DIR_EXISTS= test -d
MKDIR         = mkdir -p

####### Output default

default:all

####### Implicit rules

.SUFFIXES: .o .c .cpp .cc .cxx .C

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.C.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o "$@" "$<"

%.d: 
	@set -e;rm -f $@; \
	$(CC) -MM $(CFLAGS) $(INCPATH) $(wildcard $(@:.d=.c) $(@:.d=.cpp)) > $@.1 ; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.1 > $@; \
	rm -f $@.1

####### Build rules

objs-y := $(patsubst %,%-y,$(TARGETS))
objs   := $(foreach n,$(objs-y),$($(n)))

-include $(objs:.o=.d)

prepare:

${TARGETS}: $(objs)
#$(AR) $@ $($@-y)
	$(CXX) $(LDFLAGS) $(LIBS) -o $@ $($@-y)
	$(STRIP) --strip-unneeded $@

all: prepare ${TARGETS} 

clean:
	-$(DEL_FILE) $(objs) $(objs:.o=.d)
	-$(DEL_FILE) *~ core *.core ${TARGETS}


####### Sub-libraries

distclean: clean

.PHONY : prepare clean distclean
