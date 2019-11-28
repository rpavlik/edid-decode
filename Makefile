bindir ?= /usr/bin
mandir ?= /usr/share/man

SOURCES = edid-decode.cpp parse-base-block.cpp parse-cta-block.cpp parse-displayid-block.cpp

all: edid-decode

edid-decode: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -g -Wall -o $@ $^ -lm

$(SOURCES): edid-decode.h

clean:
	rm -f edid-decode

install:
	mkdir -p $(DESTDIR)$(bindir)
	install -m 0755 edid-decode $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)/man1
	install -m 0644 edid-decode.1 $(DESTDIR)$(mandir)/man1
