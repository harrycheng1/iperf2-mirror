noinst_LIBRARIES = libcompat.a

AM_CPPFLAGS = @STRIP_BEGIN@ \
	      -I$(top_srcdir)/include \
	      -I$(top_builddir)/include \
	      @STRIP_END@

AM_CXXFLAGS = -Wall
AM_CFLAGS = -Wall

CLEANFILES = *.o

if DEBUG_SYMBOLS
AM_CFLAGS += -g3 -O0
AM_CXXFLAGS += -g3 -O0
else
AM_CFLAGS += -O2
AM_CXXFLAGS += -O2
endif
if ENABLE_INTERLEAVED_ASM
AM_CFLAGS += -Wall -fverbose-asm -save-temps
AM_CXXFLAGS += -Wall -fverbose-asm -save-temps
# Define additional files to clean when -save-temps is used
CLEANFILES += *.i *.s *.ii *.bc
endif

AM_LDFLAGS = -lrt

libcompat_a_SOURCES = \
		      Thread.c \
		      error.c \
		      delay.c \
		      gettimeofday.c \
		      gettcpinfo.c \
		      inet_ntop.c \
		      inet_pton.c \
		      signal.c \
		      snprintf.c \
		      string.c
