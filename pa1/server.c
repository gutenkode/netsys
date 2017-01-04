#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define MAXBUFSIZE 100
#define LS_SIZE 1000

#include "utils.c"

void loop(int sock) {
	char buffer[MAXBUFSIZE];
	int nbytes;

	bzero(buffer,sizeof(buffer));
	struct sockaddr_in from_addr;
	unsigned int addr_length = sizeof(struct sockaddr);

	printf("Server starting...\n");
	do
	{
		bzero(buffer,sizeof(buffer));
		nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, &addr_length);
		printf("The client says: %s\n", buffer);

		// ls, get <file>, put <file>, exit
		if (!strcmp(buffer,"ls"))
		{
			printf("Running ls...\n");
			char msg[LS_SIZE];
			int ind = 0;
			FILE* fp = popen("/bin/ls", "r");
			while (ind != -1 && fgets(msg+ind, LS_SIZE-ind, fp) != NULL) {
				ind = strlen(msg); // terrible
				if (ind >= LS_SIZE-1) {
					printf("Error: buffer for storing ls output is too small.\n");
					char* error = "Error: buffer for storing ls output is too small.";
					nbytes = sendto(sock, error, strlen(error), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
					ind = -1;
				}
			}
			if (ind != -1) {
				printf("%s\n",msg);
				nbytes = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
			}
		}
		else if (!strncmp(buffer,"get ",4))
		{
			sendFile(sock, buffer+4, from_addr);
		}
		else if (!strncmp(buffer,"put ",4))
		{
			receiveFile(sock, "rsrv_");
			// disable timeout after call to receiveFile
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			    printf("Error setting timeout.\n");
			}
		}
		else if (strcmp(buffer,"exit"))
		{
			char* msg = "Invalid command.";
			printf("%s\n",msg);
			nbytes = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
		}
	}
	while (strcmp(buffer,"exit"));
	printf("Server exiting...\n");
}

int main (int argc, char * argv[])
{
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length;         //length of the sockaddr_in structure
	//int nbytes;                        //number of bytes we receive in our message
	//char buffer[MAXBUFSIZE];             //a buffer to store our received message
	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	/******************
	This code populates the sockaddr_in struct with
	the information about our socket
	******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine


	//Causes the system to create a generic socket of type UDP (datagram)
	//if ((sock = **** CALL SOCKET() HERE TO CREATE UDP SOCKET ****) < 0)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) // a UDP socket
	{
		printf("Unable to create socket.\n");
	}


	/******************
	Once we've created a socket, we must bind that socket to the
	local address and port we've supplied in the sockaddr_in struct
	******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("Unable to bind socket.\n");
	}

	remote_length = sizeof(remote);

	loop(sock);
	/*
	//waits for an incoming message
	bzero(buffer,sizeof(buffer));
	//nbytes = nbytes = **** CALL RECVFROM() HERE ****;
	struct sockaddr_in from_addr;
	unsigned int addr_length = sizeof(struct sockaddr);
	bzero(buffer,sizeof(buffer));
	nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, &addr_length);

	printf("The client says %s\n", buffer);

	char msg[] = "orange";
	//nbytes = sendMessage(from_addr, msg);
	//nbytes = **** CALL SENDTO() HERE ****;
	nbytes = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
	*/
	close(sock);
}
