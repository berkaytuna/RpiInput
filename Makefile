OUT = main
SRC = main.cpp civetweb.c bcm2835.c
CC = g++
CFLAGS = -o
LFLAGS = -lpthread

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(OUT) $(LFLAGS) $(SRC)