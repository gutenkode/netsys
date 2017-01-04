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
#define SENDSIZE 2000

char* readFile(char* filename, int* size) {
	char *source = NULL;
	FILE *fp = fopen(filename, "r");
	if (fp != NULL) {
		if (fseek(fp, 0L, SEEK_END) == 0) {
			// get the size of the file
			long bufsize = ftell(fp);
			if (bufsize == -1) { return NULL; }

			*size = sizeof(char) * bufsize;
			source = malloc(*size);

			if (fseek(fp, 0L, SEEK_SET) != 0) { return NULL; }

			// actually read the file
			fread(source, sizeof(char), bufsize, fp);
			if (ferror(fp) != 0) {
				return NULL;
			}
		}
		fclose(fp);
	}
	return source;
}

void sendFile(int sock, char* filename, struct sockaddr_in from_addr) {
	int filesize = -1;
	int nbytes;
	//struct sockaddr_in from_addr;
	unsigned int addr_length = sizeof(struct sockaddr);

	char* file = readFile(filename, &filesize);

	if (file != NULL) {
		printf("Sending file %s...\n",filename);

		// send the filename
		nbytes = sendto(sock, filename, strlen(filename), 0, (struct sockaddr *)&from_addr, addr_length);
		// send the filesize
		//int filesize = MAXFILESIZE;
		printf("File size: %d\n",filesize);
		nbytes = sendto(sock, &filesize, sizeof(int), 0, (struct sockaddr *)&from_addr, addr_length);

		// send the file in chunks
		int i;
		for (i = 0; i < filesize/SENDSIZE; i++) {
			nbytes = sendto(sock, file+i*SENDSIZE, SENDSIZE, 0, (struct sockaddr *)&from_addr, addr_length);
			for (int j = 0; j < 9999; j++); // inefficient way to help prevent packets being lost by receiver
		}
		nbytes = sendto(sock, file+i*SENDSIZE, filesize-i*SENDSIZE, 0, (struct sockaddr *)&from_addr, addr_length);

		printf("File sent!\n");
		free(file);
	}
	else
	{
		printf("Invalid filename.\n");
		char* msg = "!!invalid file error!!";
		nbytes = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&from_addr, addr_length);
	}
}

void writeFile(char* filename, char* filedata, int size) {
	FILE* fp = fopen(filename, "w");
	fwrite (filedata , sizeof(char), size, fp);
	fclose(fp);
}
void receiveFile(int sock, char* append) {
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 200000; // 200ms timeout
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
	    printf("Error setting timeout.\n");
	}

	int nbytes;
	printf("Waiting for file...\n");

	struct sockaddr_in from_addr;
	unsigned int addr_length = sizeof(struct sockaddr);

	// get filename
	char namebuffer[MAXBUFSIZE];
	bzero(namebuffer,sizeof(namebuffer));
	nbytes = recvfrom(sock, namebuffer, sizeof(namebuffer), 0, (struct sockaddr *)&from_addr, &addr_length);

	if (nbytes == -1 || !strcmp(namebuffer, "!!invalid file error!!")) {
		printf("Invalid filename.\n");
		return;
	}
	printf("Receiving file '%s'...\n",namebuffer);

	// get filesize
	int filesize = -1;
	nbytes = recvfrom(sock, &filesize, sizeof(int), 0, (struct sockaddr *)&from_addr, &addr_length);
	if (nbytes == -1 || filesize == -1) {
		printf("Error receiving filesize.\n");
		return;
	}
	printf("File size: %d\n",filesize);

	// get file in chunks
	char* filebuffer = malloc(filesize);
	bzero(filebuffer,sizeof(filebuffer));
	int i;
	for (i = 0; i < filesize/SENDSIZE; i++) {
		nbytes = recvfrom(sock, filebuffer+i*SENDSIZE, SENDSIZE, 0, (struct sockaddr *)&from_addr, &addr_length);
		if (nbytes == -1) {
			printf("Timeout while waiting for packet, transfer failed.\n");
			return;
		} else
			printf("Received packet %d of %d, size %d...\n",i+1,filesize/SENDSIZE+1,SENDSIZE);
	}
	nbytes = recvfrom(sock, filebuffer+i*SENDSIZE, filesize-i*SENDSIZE, 0, (struct sockaddr *)&from_addr, &addr_length);
	if (nbytes == 0) {
		printf("Timeout while waiting for packet, transfer failed.\n");
		return;
	} else
		printf("Received packet %d of %d, size %d...\n",i+1,filesize/SENDSIZE+1,filesize-i*SENDSIZE);

	//printf("File:\n\"%s\"\n", filebuffer);
	printf("File received!\n");
	// write file with temp name
	char fullname[strlen(append)+strlen(namebuffer)];
	bzero(fullname,sizeof(fullname));
	strcat(fullname, append);
	strcat(fullname, namebuffer);
	writeFile(fullname, filebuffer, filesize);

	free(filebuffer);
}
