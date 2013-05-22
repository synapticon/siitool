# project file for siicode

CC = gcc
LD = gcc

WARNINGS = -Wall -pedantic
CFLAGS = -g $(WARNINGS) -O2 -std=gnu99
LDFLAGS = -g  $(WARNINGS)

TARGET = siicode
OBJECTS = main.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f $(TARGET) $(OBJECTS)

