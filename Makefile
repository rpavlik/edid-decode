bindir ?= /usr/bin
mandir ?= /usr/share/man

SOURCES = edid-decode.cpp parse-base-block.cpp parse-cta-block.cpp \
	  parse-displayid-block.cpp parse-ls-ext-block.cpp

all: edid-decode

edid-decode: $(SOURCES) edid-decode.h version.h
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -g -Wall -o $@ $(SOURCES) -lm

version.h:
	@if [ -d .git ]; then \
		echo -n "#define SHA " >$@ ; \
		git rev-parse HEAD >>$@ ; \
	else \
		touch $@ ; \
	fi

clean:
	rm -f edid-decode version.h

install:
	mkdir -p $(DESTDIR)$(bindir)
	install -m 0755 edid-decode $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)/man1
	install -m 0644 edid-decode.1 $(DESTDIR)$(mandir)/man1
