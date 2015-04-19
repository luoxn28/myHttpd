all: server

server: server.c
	gcc -W -Wall -lpthread -o server server.c -lpython2.7

clean:
	rm server
