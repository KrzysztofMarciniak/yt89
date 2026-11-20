CC      ?= cc
CFLAGS  ?= -std=c89 -pedantic -Wall -O2
PREFIX  ?= /usr/local

all: yt89

yt89: yt89.c
	$(CC) $(CFLAGS) -o $@ yt89.c

install: yt89
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp yt89 $(DESTDIR)$(PREFIX)/bin/yt89
	chmod 755 $(DESTDIR)$(PREFIX)/bin/yt89

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/yt89

clean:
	rm -f yt89

.PHONY: all install uninstall clean
