#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h> // read, write, close

#include "utils.cpp"

using namespace std;

int openServerSocket(string hostname, int portnum) {
    int sock;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error("Error opening socket, line "+to_string(__LINE__), sock);
    server = gethostbyname(hostname.c_str());
    if (server == NULL) {
        server = gethostbyname(("www."+hostname).c_str());
        if (server == NULL) {
            cout << "No such host: '" << hostname << "'\n";
            close(sock);
            return -1;
            //error("No such host, line "+to_string(__LINE__), sock);
        }
    }
    cout << "Found host: '" << hostname << "'\n";

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(portnum);

    int err = connect(sock,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (err < 0) {
        cout << "Failed to connect to server " << hostname << ":" << portnum << "\n";//error("Error connecting, line "+to_string(__LINE__));
        close(sock);
        return -1;
    }

    return sock;
}

void sendError(int sock) {
    string msg = "HTTP/1.0 400 Bad Request\n<html><body>400 Bad Request</body></html>\n\n";
    int err = send(sock, msg.c_str(), msg.length(),0);
    if (err < 0) {
        error("Error writing to socket, line "+to_string(__LINE__), sock);
    }
}

void* threadStart(void* arg)
{
    int err;
    int sock = *((int*)arg);
    int buffersize = 4096;
    char* buffer = new char[buffersize];

    // read client's request
    bzero(buffer,buffersize);
    err = recv(sock,buffer,buffersize-1,0);
    if (err < 0)
        error("Error reading from socket, line "+to_string(__LINE__), sock);
    buffer[err] = '\n';

    // parse hostname and open connection
    string host;
    vector<string> req = split(buffer, '\n');
    vector<string> req2 = split(req[0].c_str(), ' ');

    if (req2.size() != 3 || req2[0] != "GET") {
        sendError(sock);
        cout << "Closing client connection: " << sock << "\n";
        delete[] buffer;
        close(sock);
        pthread_exit(0);
    }

    bool keepalive = false;
    host = req2[1];
    cout << "Host in header: '" << host << "'\n";
    for (string s : req) {
        if (s.substr(0,6) == "Host: ") {
            host = s.substr(6,s.length()-7); // 7 = 6+1
            cout << "Host line: '" << host << "'\n";
        } else if (s.substr(0,12) == "Connection: ") {
            if (s.substr(12,s.length()-13) == "keep-alive")
                keepalive = true;
            cout << "Connection is keep-alive.\n";
        }
        //if (s.length() > 0)
        //    cout << "*** " << s << "\n";
    }
    //cout << "Host: " << host << "\n";

    if (host.substr(0,7) == "http://")
        host = host.substr(7);
    int newsock = openServerSocket(host, 80);

    if (newsock == -1) {
        cout << "Closing client connection: " << sock << "\n";
        delete[] buffer;
        close(sock);
        pthread_exit(0);
    }

    struct timeval tv;
    tv.tv_sec = 5;  // timeout in seconds
    tv.tv_usec = 0;
    setsockopt(newsock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    // write client's request to server
    cout << "Writing to server...\n";
    err = send(newsock, buffer, err+1,0); // err is still the size of received data from client's request
    if (err < 0) {
        close(newsock);
        error("Error writing to socket, line "+to_string(__LINE__), sock);
    }

    bool loop = keepalive;
    do {
        // receive server's response
        cout << "Waiting for server...\n";
        bzero(buffer,buffersize);
        err = recv(newsock,buffer,buffersize-1,0);
        if (err <= 0) {
            cout << "No response from server, closing connection.\n";
            loop = false;
            //close(newsock);
            //error("Error reading from socket, line "+to_string(__LINE__), sock);
        } else {
            cout << "Received " << err << " bytes from server.\n";
            /*
            // print the response
            cout << "Writing to client...\n";
            vector<string> response = split(buffer, '\n');
            for (string s : response) {
                if (s.length() > 0)
                    cout << "*** " << s << "\n";
            }
            */
            // write server response to client
            err = send(sock, buffer, err,0);
            if (err < 0) {
                cout << "Failed to send to client, closing connection.\n";
                loop = false;
                //close(newsock);
                //error("Error writing to socket, line "+to_string(__LINE__), sock);
            }
        }
    } while (loop);

    delete[] buffer;
    cout << "Closing client connection: " << sock << "\n";
    close(newsock);
    close(sock);
    pthread_exit(0);
}

// Main loop to listen for clients
void loopMultithread(int sock)
{
    struct sockaddr_in cli_addr;
    while (true)
    {
        listen(sock, 5);

        socklen_t clilen = sizeof(cli_addr);
        int newsock = accept(sock, (struct sockaddr *) &cli_addr, &clilen);
        if (newsock < 0)
            error("Error accepting socket, line "+to_string(__LINE__), newsock);

        cout << "\nOpening client connection: " << newsock << "\n";
        //cout << newsock << " Starting thread...\n\n";
        pthread_t thread;
        int err = pthread_create(&thread, NULL, threadStart, &newsock);
        if (err) {
            close(newsock);
            error("Unable to start thread.", sock);
        }
    }
}

// Initialize a server listening on a port, and return the int handle of the bound socket.
int initServer(int portNum)
{
    struct sockaddr_in serv_addr;
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;           // address family
	serv_addr.sin_port = htons(portNum);      // htons() sets the port # to network byte order
	serv_addr.sin_addr.s_addr = INADDR_ANY;   // supplies the IP address of the local machine

    // create a TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error("Unable to create socket, line "+to_string(__LINE__), sock);

    // bind the socket to the address
    socklen_t srvlen = sizeof(serv_addr);
    if (::bind(sock, (struct sockaddr *) &serv_addr, srvlen) < 0)
        error("Unable to bind socket, line "+to_string(__LINE__), sock);
    return sock;
}

int globalSock;
void signal_callback_handler(int signum)
{
   cout << "\nCaught signal " << signum << "\n";
   close(globalSock);
   exit(signum);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
        error("Usage: <port>",-1);

    cout << "Starting proxy...\n";
    signal(SIGINT, signal_callback_handler);

    int sock = initServer(stoi(argv[1]));
    globalSock = sock;

    loopMultithread(sock);
    close(sock);
}
