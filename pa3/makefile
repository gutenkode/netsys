CC=g++

all: server client

server: server.cpp utils.cpp
	$(CC) server.cpp -o server

client: client.cpp utils.cpp
	$(CC) client.cpp -o client

clean:
	rm server
	rm client
