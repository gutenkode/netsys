all: server client

server: server.c utils.c
	gcc server.c -o server

client: client.c utils.c
	gcc client.c -o client

clean:
	rm server
	rm client
