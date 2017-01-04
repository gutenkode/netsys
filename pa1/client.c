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
#include <errno.h>

#define MAXBUFSIZE 100
#define LS_SIZE 1000

#include "utils.c"

void loop(int sock, struct sockaddr_in remote) {
	int nbytes;
	char command[MAXBUFSIZE];

	printf("Client starting...\n");
	do
	{
		bzero(command,sizeof(command));
		printf("> ");
		gets(command);
		nbytes = sendto(sock, command, strlen(command), 0, (struct sockaddr *)&remote, sizeof(remote));

		if (!strncmp(command,"get ",4))
		{
			receiveFile(sock, "rcln_");
			// disable timeout after call to receiveFile
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			    printf("Error setting timeout.\n");
			}
		}
		else if (!strncmp(command,"put ",4))
		{
			sendFile(sock, command+4, remote);
		}
		else if (strcmp(command,"exit"))
		{
			// Blocks till bytes are received
			struct sockaddr_in from_addr;
			unsigned int addr_length = sizeof(struct sockaddr);
			char buffer[LS_SIZE];
			bzero(buffer,sizeof(buffer));
			nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, &addr_length);

			printf("Server response:\n%s\n", buffer);
		}

	}
	while(strcmp(command,"exit"));
	printf("Client exiting...\n");
}

int main (int argc, char * argv[])
{

	//int nbytes;                             // number of bytes send by sendto()
	int sock;                               //this will be our socket
	//char buffer[MAXBUFSIZE];

	struct sockaddr_in remote;              //"Internet socket address structure"

	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}

	/******************
	Here we populate a sockaddr_in struct with
	information regarding where we'd like to send our packet
	i.e the Server.
	******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
	//if ((sock = **** CALL SOCKET() HERE TO CREATE A UDP SOCKET ****) < 0)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) // a UDP socket
	{
		printf("Unable to create socket.\n");
	}

	loop(sock, remote);
	/******************
	sendto() sends immediately.
	it will report an error if the message fails to leave the computer
	however, with UDP, there is no error if the message is lost in the network once it leaves the computer.
	******************/
	/*
	char command[] = "apple";
	//nbytes = **** CALL SENDTO() HERE ****;
	nbytes = sendto(sock, command, strlen(command), 0, (struct sockaddr *)&remote, sizeof(remote));

	// Blocks till bytes are received
	struct sockaddr_in from_addr;
	unsigned int addr_length = sizeof(struct sockaddr);
	bzero(buffer,sizeof(buffer));
	//nbytes = **** CALL RECVFROM() HERE ****;
	nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, &addr_length);

	printf("Server says %s\n", buffer);
	*/
	close(sock);

}
