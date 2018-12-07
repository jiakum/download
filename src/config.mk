####### Compiler, tools and options

CC            = gcc
CXX           = g++
DEFINES       = 
CFLAGS        += -m64 -pipe -O2 -Wall -Wno-unused-parameter -W -D_REENTRANT $(DEFINES)
CXXFLAGS      += -m64 -pipe -O2 -Wall -Wno-unused-parameter -W -D_REENTRANT $(DEFINES)
INCPATH       += -I./include -I. -I/usr/include/glib-2.0/ -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
LINK          = g++
LFLAGS        += -m64 -Wl,-O1
LIBS          += $(SUBLIBS)  -L/usr/lib/x86_64-linux-gnu -lrt -lpthread 
AR            = ar cqs
RANLIB        = 
QMAKE         = /usr/lib/x86_64-linux-gnu/qt4/bin/qmake
TAR           = tar -cf
COMPRESS      = gzip -9f
COPY          = cp -f
SED           = sed
COPY_FILE     = $(COPY)
COPY_DIR      = $(COPY) -r
STRIP         = strip
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
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $($@-y)
	$(STRIP) --strip-unneeded $@

all: prepare ${TARGETS} 

clean:
	-$(DEL_FILE) $(objs) $(objs:.o=.d)
	-$(DEL_FILE) *~ core *.core ${TARGETS}


####### Sub-libraries

distclean: clean

.PHONY : prepare clean distclean
