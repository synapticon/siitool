# project file for siitool

CC = gcc
LD = gcc

WARNINGS = -Wall -Wextra
OPTIMIZATION = -O2
DEBUG = 

CFLAGS = -g $(WARNINGS) $(OPTIMIZATION) -std=gnu99 $(DEBUG)
LDFLAGS = -g  $(WARNINGS)

# add libxml specific flags
CFLAGS += `xml2-config --cflags`
LDFLAGS += `xml2-config --libs`

H2MFLAGS = --help-option "-h" --version-option "-v" --no-discard-stderr --no-info

TARGET = siitool
OBJECTS = main.o sii.o esi.o esifile.o

PREFIX = /usr/local/bin
MANPATH = ../man/man1

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

all: $(TARGET) $(TARGET).1

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

$(TARGET).1: $(TARGET)
	help2man -o $<.1 $(H2MFLAGS) -i misc/mansections.txt ./$<

.PHONY: clean install install-man install-prg uninstall lint

install: install-man install-prg

install-prg:
	strip $(TARGET)
	install $(TARGET) $(PREFIX)

install-man:
	install $(TARGET).1 $(PREFIX)/$(MANPATH)
	@mandb

uninstall:
	rm -i $(PREFIX)/$(TARGET)
	rm -i $(PREFIX)/$(MANPATH)/$(TARGET).1

clean:
	rm -f $(TARGET) $(TARGET).1 $(OBJECTS)

lint:
	clang --analyze `xml2-config --cflags` main.c sii.c esi.c esifile.c
