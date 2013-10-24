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

TARGET = siitool
OBJECTS = main.o sii.o esi.o esifile.o

PREFIX = /usr/local/bin

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean install lint

install:
	install $(TARGET) $(PREFIX)

clean:
	rm -f $(TARGET) $(OBJECTS)

lint:
	clang --analyze `xml2-config --cflags` main.c sii.c esi.c esifile.c
