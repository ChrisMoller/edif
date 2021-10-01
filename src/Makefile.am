
lib_LTLIBRARIES = libedif.la libedif2.la

libedif_la_SOURCES = edif.cc
libedif_la_CPPFLAGS = -I$(APL_SOURCES) -I$(APL_SOURCES)/src

libedif2_la_SOURCES = edif2.cc edif2.hh
libedif2_la_LDFLAGS = $(LIBNOTIFY_LIBS) -lrt -pthread
libedif2_la_CPPFLAGS = -I$(APL_SOURCES) -I$(APL_SOURCES)/src \
          $(LIBNOTIFY_CFLAGS) -pthread

noinst_LTLIBRARIES =

BUILT_SOURCES = gitversion.h

.FORCE:

gitversion.h : .FORCE
	git log  -1 --pretty='format:#define GIT_VERSION "%h %ci"' > xxx
	echo "" >> xxx
	cmp -s xxx $@; \
	rc=$$?; \
	if [ $$rc -eq 0 ]; then rm xxx; else mv xxx $@; fi
