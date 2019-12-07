bindir ?= /usr/bin
mandir ?= /usr/share/man

SOURCES = edid-decode.cpp parse-base-block.cpp parse-cta-block.cpp \
	  parse-displayid-block.cpp parse-ls-ext-block.cpp \
	  parse-di-ext-block.cpp parse-vtb-ext-block.cpp
WARN_FLAGS = -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter

all: edid-decode

sha = -DSHA=$(shell if [ -d .git ]; then git rev-parse HEAD ; else printf '"not available"'; fi)

edid-decode: $(SOURCES) edid-decode.h Makefile
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(WARN_FLAGS) -g $(sha) -o $@ $(SOURCES) -lm

clean:
	rm -f edid-decode

install:
	mkdir -p $(DESTDIR)$(bindir)
	install -m 0755 edid-decode $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)/man1
	install -m 0644 edid-decode.1 $(DESTDIR)$(mandir)/man1
