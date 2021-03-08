OUT = main
SRC = main.cpp src/civetweb.c src/bcm2835.c
CC = g++
CFLAGS = -o
IFLAGS = -Iinc
LFLAGS = -lpthread

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(OUT) $(IFLAGS) $(LFLAGS) $(SRC)
