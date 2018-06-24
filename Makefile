
TARGET = usb-widget
LIBS = `pkg-config --libs gtk+-3.0`
CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0` -I. 
CFLAGS += -Wextra -Wformat-contains-nul -Wformat-extra-args -Wformat-security -Wformat-signedness -Wformat-y2k -Wformat-zero-length
CFLAGS += -Werror -Wno-deprecated-declarations -Wstrict-prototypes -Werror=unused-variable -Werror=unused-but-set-variable -Werror=unused-function -Wsuggest-attribute=format
ifneq ($(DEBUG),)
CFLAGS += -O0 -g -ggdb -rdynamic
endif

.PHONY: default all clean

default: $(TARGET)
all: default

#OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
OBJECTS = main.o usb-device-manager.o usb-device-redir-widget.o

#HEADERS = $(wildcard *.h)
HEADERS = usb-device-manager.h usb-device-widget.h spice-client.h config.h

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o $(TARGET)
