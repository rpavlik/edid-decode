bindir ?= /usr/bin
mandir ?= /usr/share/man

EMXX ?= em++

SOURCES = edid-decode.cpp parse-base-block.cpp parse-cta-block.cpp \
	  parse-displayid-block.cpp parse-ls-ext-block.cpp \
	  parse-di-ext-block.cpp parse-vtb-ext-block.cpp calc-gtf-cvt.cpp
WARN_FLAGS = -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter

all: edid-decode

sha = -DSHA=$(shell if test -d .git ; then git rev-parse --short=12 HEAD ; fi)
date = -DDATE=$(shell if test -d .git ; then printf '"'; TZ=UTC git show --quiet --date='format-local:%F %T"' --format="%cd"; fi)

edid-decode: $(SOURCES) edid-decode.h Makefile
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(WARN_FLAGS) -g $(sha) $(date) -o $@ $(SOURCES) -lm

edid-decode.js: $(SOURCES) edid-decode.h Makefile
	$(EMXX) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(WARN_FLAGS) $(sha) $(date) -s EXPORTED_FUNCTIONS='["_parse_edid"]' -s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' -o $@ $(SOURCES) -lm

clean:
	rm -f edid-decode

install:
	mkdir -p $(DESTDIR)$(bindir)
	install -m 0755 edid-decode $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)/man1
	install -m 0644 edid-decode.1 $(DESTDIR)$(mandir)/man1
