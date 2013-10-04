# project file for siitool

CC = gcc
LD = gcc

WARNINGS = -Wall -Wextra
OPTIMIZATION = -O2
DEBUG = 

CFLAGS = -g $(WARNINGS) $(OPTIMIZATION) -std=gnu99 $(DEBUG)
LDFLAGS = -g  $(WARNINGS)

TARGET = siitool
OBJECTS = main.o sii.o

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
	splint -weak +posixlib -skipposixheaders -skipisoheaders +trytorecover  main.c
