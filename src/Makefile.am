
lib_LTLIBRARIES = libedif.la libedif2.la

libedif_la_SOURCES = edif.cc
libedif_la_CPPFLAGS = -I$(APL_SOURCES)

libedif2_la_SOURCES = edif2.cc edif2.hh
libedif2_la_LDFLAGS = -lrt -pthread
libedif2_la_CPPFLAGS = -I$(APL_SOURCES) -pthread

noinst_LTLIBRARIES =

BUILT_SOURCES = gitversion.h

.FORCE:

gitversion.h : .FORCE
	git log  -1 --pretty='format:#define GIT_VERSION "%h %ci"' > $@
