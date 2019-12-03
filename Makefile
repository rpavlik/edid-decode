bindir ?= /usr/bin
mandir ?= /usr/share/man

SOURCES = edid-decode.cpp parse-base-block.cpp parse-cta-block.cpp \
	  parse-displayid-block.cpp parse-ls-ext-block.cpp \
	  parse-di-ext-block.cpp
WARN_FLAGS = -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter

all: edid-decode

edid-decode: $(SOURCES) edid-decode.h
	@if [ -d .git ]; then \
		printf "#define SHA " >version.h; \
		git rev-parse HEAD >>version.h; \
	else \
		echo >version.h; \
	fi
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(WARN_FLAGS) -g -o $@ $(SOURCES) -lm

clean:
	rm -f edid-decode version.h

install:
	mkdir -p $(DESTDIR)$(bindir)
	install -m 0755 edid-decode $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)/man1
	install -m 0644 edid-decode.1 $(DESTDIR)$(mandir)/man1
