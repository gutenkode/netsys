/*
Server should:
listen for and receive GET, PUT, LIST
return appropriate files and data
*/

#include <iostream>
#include <map>

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

// Parse a request from a client, stored in buffer
void parseRequest(int sock, char* msg, string dir)
{
    int err;
    cout << "Request received: '" << msg << "'\n";
    if (strcmp(msg, "EXIT") == 0)
    {}
    else if (strncmp(msg, "GET ", 4) == 0)
    {
        string filename = dir+(msg+4);
        cout << "Looking for file '" << filename << "'\n";
        int filesize = -1;
        char* file = readFile(filename, &filesize);

        // send filesize
        if (file == NULL)
            filesize = -1;
        err = write(sock, &filesize, sizeof(int));
        if (err < 0)
            error("Error writing to socket, line "+to_string(__LINE__), sock);

        if (file != NULL)
        {
            cout << "Sending file...\n";
            err = write(sock, file, filesize);
            if (err < 0)
                error("Error writing to socket, line "+to_string(__LINE__), sock);

            delete[] file;
        }
        else {
            cout << "No such file.\n";
        }
    }
    else if (strncmp(msg, "PUT ", 4) == 0)
    {
        string data = (msg+4);
        vector<string> dataparts = split(data.c_str(),'\\');
        string filename = dir+dataparts[0];
        int filesize = stoi(dataparts[1].c_str());
        string nameAppend = "."+dataparts[2];

        // receive file
        char* buffer = new char[filesize];
        err = read(sock,buffer,filesize);
        if (err < 0)
            error("Error reading from socket, line "+to_string(__LINE__), sock);

        cout << "Writing file: " << filename << "\n";
        writeFile(filename+nameAppend, buffer,filesize);

        delete[] buffer;
    }
    else if (strncmp(msg, "LIST", 4) == 0)
    {
        int length = -1;
        const char* response = listDir(dir, &length);
        err = write(sock, response, length);
        delete[] response;
        if (err < 0)
            error("Error writing to socket, line "+to_string(__LINE__), sock);
    }
    else
    {
        string response = "Unrecognized command.";
        err = write(sock, response.c_str(), response.length());
        if (err < 0)
            error("Error writing to socket, line "+to_string(__LINE__), sock);
    }
}

// Listen to client requests and parse them until EXIT is received
void handleClient(int sock, string dir)
{
    int err, buffersize = 1024;
    char* buffer = new char[buffersize];

    do {
        bzero(buffer,buffersize);
        err = read(sock,buffer,buffersize-1);
        if (err < 0)
            error("Error reading from socket, line "+to_string(__LINE__), sock);
        parseRequest(sock, buffer, dir);
    } while (strcmp(buffer, "EXIT") != 0);

    delete[] buffer;
}

// Verify user credentials, username string on success and empty string on failure
string loginClient(int sock, map<string,string> userMap)
{
    int err, buffersize = 64;
    char* buffer = new char[buffersize];

    bzero(buffer,buffersize);
    err = read(sock,buffer,buffersize-1);
    if (err < 0)
        error("Error reading from socket, line "+to_string(__LINE__), sock);
    cout << "Username: " << buffer << "\n";

    int result = 1;
    string username = string(buffer);
    if (userMap.find(username) == userMap.end())
        result = 0;
    err = write(sock, &result, sizeof(int));
    if (err < 0)
        error("Error writing to socket, line "+to_string(__LINE__), sock);
    if (result != 1) {
        delete[] buffer;
        return "";
    }

    bzero(buffer,buffersize);
    err = read(sock,buffer,buffersize-1);
    if (err < 0)
        error("Error reading from socket, line "+to_string(__LINE__), sock);
    cout << "Password: " << buffer << "\n";

    result = 1;
    if (userMap.find(username)->second != string(buffer))
        result = 0;
    err = write(sock, &result, sizeof(int));
    if (err < 0)
        error("Error writing to socket, line "+to_string(__LINE__), sock);

    delete[] buffer;

    if (result != 1)
        return "";
    return username;
}

// Continuously listen on a specified port, and parese requests
void loop(int sock, map<string,string> userMap)
{
    struct sockaddr_in cli_addr;
    //while (true) // temporary
    {
        listen(sock, 5);

        socklen_t clilen = sizeof(cli_addr);
        int newsock = accept(sock, (struct sockaddr *) &cli_addr, &clilen);
        if (newsock < 0)
            error("Error accepting socket, line "+to_string(__LINE__), newsock);

        cout << "Opening client connection: " << newsock << "\n";
        string username = loginClient(newsock, userMap);
        system(("mkdir "+username+"/").c_str());
        if (username != "") // just gonna assume that nobody will ever have empty string as their username...
            handleClient(newsock, username+"/");
        else
            cout << "Invalid credentials.\n";
        cout << "Closing client connection: " << newsock << "\n";

        close(newsock);
    }
}

struct threadArgs {
    int sock;
    string username;
};
// Code for managing a client in loopMultithread
void* threadStart(void* arg)
{
    threadArgs args = *((threadArgs*)arg);
    //int newsock = *((int*)arg);

    handleClient(args.sock, args.username+"/");

    cout << "Closing client connection: " << args.sock << "\n";
    close(args.sock);

    pthread_exit(0);
}

// Support multiple clients
void loopMultithread(int sock, map<string,string> userMap)
{
    struct sockaddr_in cli_addr;
    while (true) // temporary
    {
        listen(sock, 5);

        socklen_t clilen = sizeof(cli_addr);
        int newsock = accept(sock, (struct sockaddr *) &cli_addr, &clilen);
        if (newsock < 0)
            error("Error accepting socket, line "+to_string(__LINE__), newsock);

        cout << "Opening client connection: " << newsock << "\n";
        string username = loginClient(newsock, userMap);
        system(("mkdir "+username+"/").c_str());
        if (username != "") // just gonna assume that nobody will ever have empty string as their username...
        {
            std::cout << newsock << " Starting thread...\n\n";
            pthread_t thread;
            threadArgs args;
            args.sock = newsock;
            args.username = username;
            int err = pthread_create(&thread, NULL, threadStart, &args);
            if (err)
                error("Unable to start thread.", sock);
        }
        else
            cout << "Invalid credentials.\n";
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

map<string,string> parseConfig()
{
    int filesize = -1;
    char* file = readFile("dfs.conf", &filesize);

    map<string,string> userMap;
    for (string s : split(file,'\n')) {
        vector<string> pair = split(s.c_str(),' ');
        userMap[pair[0]] = pair[1];
    }
    return userMap;
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
    cout << "Starting server...\n";
    signal(SIGINT, signal_callback_handler);

    if (argc != 3)
        error("needs arguments", -1);
    string serverdir = string(argv[1]+1)+"/";
    cout << "Directory: " << argv[1] << "\nPort: " << argv[2] << "\n";

    map<string,string> userMap = parseConfig(); // read config file before changing directory

    system(("mkdir "+serverdir).c_str()); // portability? what's that?
    if (chdir(serverdir.c_str()) == -1)
        error("Error changing directory.", -1);

    int sock = initServer(stoi(argv[2]));
    globalSock = sock; // whatever

    loopMultithread(sock, userMap);
    close(sock);
}
