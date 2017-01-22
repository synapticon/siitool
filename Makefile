# project file for siitool

CC = gcc
LD = gcc

INSTALL = install

WARNINGS = -Wall -Wextra
OPTIMIZATION = -O2
DEBUG ?= 0

CFLAGS = -g $(WARNINGS) $(OPTIMIZATION) -std=gnu99 -DDEBUG=$(DEBUG)
LDFLAGS = -g  $(WARNINGS)

PLATTFORM = $(shell uname -s)

ifeq (Linux, $(PLATTFORM))
  INSTFLAGS = -D -b
else
ifeq (Darwin, $(PLATTFORM))
  INSTFLAGS = -b
else
  INSTFLAGS =
endif
endif

# add libxml specific flags
CFLAGS += `xml2-config --cflags`
LDFLAGS += `xml2-config --libs`

H2MFLAGS = --help-option "-h" --version-option "-v" --no-discard-stderr --no-info

TARGET = siitool
OBJECTS = main.o sii.o esi.o esifile.o crc8.o

DESTDIR = /usr/local/bin
ifeq (Darwin, $(PLATTFORM))
MANPATH = /usr/local/share/man/man1
else
MANPATH = $(DESTDIR)/../man/man1
endif

SOURCEDIR = `pwd`
VERSION = $(shell git describe --always)

CFLAGS += -DVERSION=\"$(VERSION)\"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

all: $(TARGET)

man: $(TARGET).1

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

$(TARGET).1:
	help2man -o $@ $(H2MFLAGS) -i misc/mansections.txt ./${TARGET}

.PHONY: clean cleanall install install-man install-prg uninstall lint tarball help

help:
	@echo "Available make targets:"
	@echo "  all        builds binary"
	@echo "  man        builds man page"
	@echo "  install    installs this software at $(DESTDIR)"
	@echo "  uninstall  removes installed software from $(DESTDIR)"
	@echo "  clean      clean all objects"
	@echo "  cleanall   clean all objects, also prebuild ones"
	@echo "  lint       static code analyzing (works best with clang v3.4+)"
	@echo "  tarball    packages a software release tar.gz file."

install: install-man install-prg

install-prg:
	strip $(TARGET)
	$(INSTALL) $(INSTFLAGS) $(TARGET) $(DESTDIR)

install-man:
	$(INSTALL) $(INSTFLAGS) $(TARGET).1 $(MANPATH)/$(TARGET).1
ifeq (Linux, $(PLATTFORM))
	@mandb -f $(MANPATH)/$(TARGET).1
endif

uninstall:
	rm -f $(DESTDIR)/$(TARGET)
	rm -f $(MANPATH)/$(TARGET).1

clean:
	rm -f $(TARGET) $(OBJECTS)

cleanall: clean
	rm -f $(TARGET).1

lint:
	clang --analyze `xml2-config --cflags` main.c sii.c esi.c esifile.c

tarball:
	git archive --format=tar --prefix="$(TARGET)-$(VERSION)/" HEAD | gzip > $(TARGET)-$(VERSION).tar.gz
