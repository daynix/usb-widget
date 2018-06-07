
TARGET = usb-widget
LIBS = `pkg-config --libs gtk+-3.0`
CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0` -I.

.PHONY: default all clean

default: $(TARGET)
all: default

#OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
OBJECTS = usb-device-manager.o main.o
#HEADERS = $(wildcard *.h)
HEADERS = usb-device-manager.h

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o $(TARGET)
