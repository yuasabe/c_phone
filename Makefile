
all:		server client

server: 	server.c
		gcc server.c -o server -Wall -lpthread

client:		client.c
		gcc client.c -o client -Wall -lpthread


