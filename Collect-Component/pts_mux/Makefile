CC = gcc
CFLAGS = -g -Wall
LIBS =
INCLUDES =

WITH_DEBUG = 1

ifeq ($(WITH_DEBUG), 1)
CFLAGS += -DDEBUG
endif

all: pts_mux

pts_mux: base64.o jsmn.o pts_mux.c
	$(CC) $(CFLAGS) -o pts_mux base64.o jsmn.o pts_mux.c $(INCLUDES) $(LIBS)

base64.o: base64.c
	$(CC) $(CFLAGS) -o base64.o -c base64.c $(INCLUDES) $(LIBS)

jsmn.o: jsmn.c
	$(CC) $(CFLAGS) -o jsmn.o -c jsmn.c $(INCLUDES) $(LIBS)

clean:
	rm -f base64.o jsmn.o pts_mux

