.PHONY: client server

all: client server

client:
	gcc -o client client.c graphing.c -lpthread -lm `sdl-config --cflags --libs` -lrt

server:
	gcc -o server server.c graphing.c -lpthread -lm `sdl-config --cflags --libs` -lrt