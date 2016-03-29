# project file for siitool

CC = gcc
LD = gcc

WARNINGS = -Wall -Wextra
OPTIMIZATION = -O2
DEBUG = 0

CFLAGS = -g $(WARNINGS) $(OPTIMIZATION) -std=gnu99 -DDEBUG=$(DEBUG)
LDFLAGS = -g  $(WARNINGS)

# add libxml specific flags
CFLAGS += `xml2-config --cflags`
LDFLAGS += `xml2-config --libs`

H2MFLAGS = --help-option "-h" --version-option "-v" --no-discard-stderr --no-info

TARGET = siitool
OBJECTS = main.o sii.o esi.o esifile.o crc8.o

PREFIX = /usr/local/bin
MANPATH = ../man/man1

SOURCEDIR = `pwd`
VERSION = `scripts/getversion`

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

all: $(TARGET) $(TARGET).1

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

$(TARGET).1: $(TARGET)
	help2man -o $<.1 $(H2MFLAGS) -i misc/mansections.txt ./$<

.PHONY: clean install install-man install-prg uninstall lint tarball help

help:
	@echo "Available make targets:"
	@echo "  all        builds binary and man page"
	@echo "  install    installs this software at $(PREFIX)"
	@echo "  uninstall  removes installed software from $(PREFIX)"
	@echo "  clean      cleans all objects"
	@echo "  lint       static code analyzing (works best with clang v3.4+)"
	@echo "  tarball    packages a software release tar.gz file."

install: install-man install-prg

install-prg:
	strip $(TARGET)
	install $(TARGET) $(PREFIX)

install-man:
	install -D $(TARGET).1 $(PREFIX)/$(MANPATH)/$(TARGET).1
	@mandb -f $(PREFIX)/$(MANPATH)/$(TARGET).1

uninstall:
	rm -f $(PREFIX)/$(TARGET)
	rm -f $(PREFIX)/$(MANPATH)/$(TARGET).1

clean:
	rm -f $(TARGET) $(TARGET).1 $(OBJECTS)

lint:
	clang --analyze `xml2-config --cflags` main.c sii.c esi.c esifile.c

tarball:
	git archive --format=tar --prefix="$(TARGET)-$(VERSION)/" HEAD | gzip > $(TARGET)-$(VERSION).tar.gz
