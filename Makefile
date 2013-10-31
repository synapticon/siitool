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

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

man: $(TARGET)
	help2man -o $<.1 $(H2MFLAGS) -i misc/mansections.txt ./$<

.PHONY: clean install install-man install-prg lint

install: install-man install-prg

install-prg:
	strip $(TARGET)
	install $(TARGET) $(PREFIX)

install-man:
	install $(TARGET).1 $(PREFIX)/../man/man1
	@mandb

clean:
	rm -f $(TARGET) $(OBJECTS)

lint:
	clang --analyze `xml2-config --cflags` main.c sii.c esi.c esifile.c
