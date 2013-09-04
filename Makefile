# project file for siicode

CC = gcc
LD = gcc

WARNINGS = -Wall -pedantic
CFLAGS = -g $(WARNINGS) -O2 -std=gnu99
LDFLAGS = -g  $(WARNINGS)

TARGET = siicode
OBJECTS = main.o sii.o

PREFIX = /usr/local/bin

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean install

install:
	install $(TARGET) $(PREFIX)

clean:
	rm -f $(TARGET) $(OBJECTS)

