
CFLAGS=-Wall -Werror
LDFLAGS=-lmosquitto

all: mqtt-chronos

mqtt-chronos: mqtt-chronos.c
	$(CC) $(CFLAGS) -o mqtt-chronos mqtt-chronos.c $(LDFLAGS)
