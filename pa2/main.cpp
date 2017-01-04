#include <iostream>
#include <sstream>
#include <vector>
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

//using namespace std;

//#define NUM_THREADS 5

static int sock, /*newsock,*/ portNum, keepalive;
static std::string docRoot, indexFile;
static std::map<std::string,std::string> typeMap;

// because I'm too lazy to keep thread of pthread_t's
class ThreadMutex {
    std::mutex m;
    int numThreads = 0;
public:
    void threadOpen() {
        m.lock();
        numThreads++;
        m.unlock();
    }
    void threadClose() {
        m.lock();
        numThreads--;
        m.unlock();
    }
    void waitForThreads() {
        // messy solution that will get the job done, but...
        bool wait = false;
        do {
            if (wait)
                sleep(1);
            wait = true;
            while(!m.try_lock()) {
                // if the the mutex is locked, wait a bit for it to free
                sleep(1);
            }
            // keep testing numThreads until all threads are closed
        } while(numThreads > 0);
        m.unlock();
    }
};
static ThreadMutex threadMutex;

void signal_callback_handler(int signum) {
   std::cout << "\nCaught signal " << signum << "\n";
   threadMutex.waitForThreads();
   close(sock);
   exit(signum);
}
void error(std::string err) {
    std::cout << err << "\n";
    close(sock);
    exit(1);
}

bool isDirectory(std::string filename) {
    struct stat path_stat;
    stat(filename.c_str(), &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

char* readFile(std::string filename, int* size)
{
    char *source = NULL;
    FILE *fp = fopen(filename.c_str(), "r");

	if (fp != NULL) {
		if (fseek(fp, 0L, SEEK_END) == 0) {
			// get the size of the file
			long bufsize = ftell(fp);
			if (bufsize == -1) { return NULL; }

			*size = sizeof(char) * bufsize;
			source = new char[*size];

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

void parseConfig(std::string s) {
    std::string delimiter = "\n";

    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        if (token.at(0) != '#') {
            std::string key = token.substr(0, s.find(" "));
            if (key == "ListenPort") {
                // parse value as default port #
                portNum = stoi(token.substr(s.find(" ")+1));
                if (portNum < 1024)
                    error("Unsupported port number: "+std::to_string(portNum));
                std::cout << "Port: " << portNum << "\n";
            } else if (key == "DocumentRoot") {
                // root directory for files
                docRoot = token.substr(s.find(" ")+2);
                docRoot = docRoot.substr(0, docRoot.length()-1);
                std::cout << "Document root: " << docRoot << "\n";
            } else if (key == "DirectoryIndex") {
                // default index file to load
                indexFile = token.substr(s.find(" ")+1);
                std::cout << "Index file: " << indexFile << "\n";
            } else if (key == "KeepaliveTime") {
                keepalive = stoi(token.substr(s.find(" ")+1));
                std::cout << "Keepalive: " << keepalive << "\n";
            } else if (key == "ContentType") {
                std::stringstream ss;
                ss.str(token.substr(s.find(" ")+1));
                std::string item;
                char delim = ' ';
                std::string values[2];
                int i = 0;
                while (std::getline(ss, item, delim)) {
                    if (item.length() != 0) {
                        values[i] = item;
                        i++;
                    }
                }
                std::cout << values[0] << " -> " << values[1] << "\n";
                typeMap[values[0]] = values[1];
            }
        }
        s.erase(0, pos + delimiter.length());
    }
}

std::string getContentType(std::string s) {
    std::string type = typeMap[s.substr(s.find_last_of("."))];
    if (type.length() == 0)
        return "text/plain";
    return type;
}

void sendError(int newsock, int errnum, std::string version, std::string body) {
    std::string response_str = version +" "+ std::to_string(errnum) +" ";
    switch (errnum) {
        case 400:
            response_str += "Bad Request";
            response_str += "\nContent-Type: text/html\nContent-Length: "+std::to_string(body.length())+"\n\n"+body;
            break;
        case 501:
            response_str += "Not Implemented";
            response_str += "\nContent-Type: text/html\nContent-Length: "+std::to_string(body.length())+"\n\n"+body;
            break;
        case 500:
            response_str += "Internal Server Error: "+body;
            break;
    }

    int responseSize = response_str.length();
    const char* response = new char[responseSize];
    memcpy((void*)response,response_str.c_str(),responseSize);
    int err = write(newsock,response,responseSize);
    delete[] response;
    if (err < 0)
        error("Error writing to socket.");
}
void sendFile(int newsock, std::string uri, std::string version) {
    int filesize = -1;
    std::string full_uri;
    if (isDirectory(docRoot+uri)) { // if this is a directory, append the index filename
        if (uri.at(uri.length()-1) == '/')
            full_uri = uri+indexFile;
        else
            full_uri = uri+"/"+indexFile; // auto-append a slash if one is missing
    } else
        full_uri = uri;
    char* file = readFile((docRoot+full_uri).c_str(), &filesize);

    const char* response;
    int responseSize = -1;
    if (file == NULL)
    {
        // invalid path, 404 error
        std::cout << "Could not read file: " << docRoot+full_uri << "\n";
        std::string body = "<html><body>404 File not found: '"+full_uri+"'</body></html>";
        std::string response_str = version+" 404 Not Found\nContent-Type: text/html\nContent-Length: "+std::to_string(body.length())+"\n\n"+body;
        responseSize = response_str.length();
        response = new char[responseSize];
        memcpy((void*)response,response_str.c_str(),responseSize);
    }
    else
    {
        // return the file, with the correct header
        std::string contentType = getContentType(full_uri);
        std::string contentLength = std::to_string(filesize-1);
        std::string header = version+" 200 OK\nContent-Type: "+contentType+"\nContent-Length: "+contentLength+"\n\n";
        responseSize = header.length()+filesize-1;
        response = new char[responseSize];
        memcpy((void*)response, header.c_str(), header.length());
        memcpy((void*)(response+header.length()), file, filesize-1);
        delete[] file;
    }
    std::cout << newsock << " Sending response...\n\n";
    //std::cout << "\nResponse:\n****\n" << response << "\n****\n";
    int err = write(newsock,response,responseSize);
    delete[] response;
    if (err < 0)
        error("Error writing to socket.");
}

void parseRequest(int newsock, std::string s) {
    std::string req_method;
    std::string req_uri;
    std::string req_version;

    try {
        try {
            size_t pos = 0;
            std::string firstline = s.substr(0, s.find("\n"));

            pos = firstline.find(" ");
            req_method = firstline.substr(0, pos);
            firstline.erase(0, pos + 1);

            pos = firstline.find(" ");
            req_uri = firstline.substr(0, pos);
            firstline.erase(0, pos + 1);

            req_version = firstline.substr(0,firstline.length()-1);
        } catch (std::exception& e) {
            // some error while parsing
            sendError(newsock, 400, req_version, "<html><body>400 Bad Request</body></html>");
            return;
        }

        if (req_method == req_uri || req_method == req_version || req_uri == req_version ||
            req_method.length() == 0 || req_uri.length() == 0 || req_version.length() == 0) {
            // possible guaranteed error states
            sendError(newsock, 400, "HTTP/1.1", "<html><body>400 Bad Request</body></html>");
            return;
        }

        std::cout << newsock << " Request:\nMethod: " << req_method << "\nURI: " << req_uri << "\nVersion: " << req_version << "\n\n";

        if (req_version != "HTTP/1.1" && req_version != "HTTP/1.0") {
            // send 400 bad request, invalid version error
            sendError(newsock, 400, "HTTP/1.1", "<html><body>400 Bad Request Reason: Invalid HTTP-Version: "+req_version+"</body></html>");
            // send 501 not implemented error if the version is valid but unsupported
        } else {
            if (req_method == "GET") {
                sendFile(newsock, req_uri, req_version);
            } else if (req_method == "HEAD" || req_method == "POST" || req_method == "DELETE" || req_method == "OPTIONS") {
                // send 501 not implemented error
                sendError(newsock, 501, req_version, "<html><body>501 Not Implemented: Method: "+req_method+"</body></html>");
            } else {
                // send 400 bad request, invalid method error
                sendError(newsock, 400, req_version, "<html><body>400 Bad Request Reason: Invalid Method: "+req_method+"</body></html>");
            }
        }
    } catch (std::exception& e) {
        // send 500 internal server error for any other possible error cases
        sendError(newsock, 500, req_version, e.what());
    }
}

void* threadStart(void* arg)
{
    threadMutex.threadOpen(); // increment thread count
    int newsock = *((int*)arg);
    int err;
    int buffersize = 1024;
    char* buffer = new char[buffersize];
    bzero(buffer,buffersize);

    err = read(newsock,buffer,buffersize-1);
    if (err < 0) {
        sendError(newsock, 500, "HTTP/1.1", "Unable to read from socket.");
        error("Error reading from accepted socket.");
    }
    parseRequest(newsock, buffer);

    delete[] buffer;
    close(newsock);
    std::cout << newsock << " Exiting thread... \n\n";
    threadMutex.threadClose(); // decrement thread count
    pthread_exit(0);
}

void loopMultithread()
{
    struct sockaddr_in cli_addr;
    while (true)
    {
        listen(sock, 5);

        socklen_t clilen = sizeof(cli_addr);
        int newsock = accept(sock, (struct sockaddr *) &cli_addr, &clilen);
        if (newsock < 0)
            error("Error accepting socket.");

        std::cout << newsock << " Starting thread...\n\n";
        pthread_t thread;
        int err = pthread_create(&thread, NULL, threadStart, (void*)&newsock);
        if (err)
            error("Unable to start thread.");
        // newsock is closed in the thread
    }
}

void loop()
{
    struct sockaddr_in cli_addr;
    int err;
    while (true)
    {
        listen(sock, 5);

        socklen_t clilen = sizeof(cli_addr);
        int newsock = accept(sock, (struct sockaddr *) &cli_addr, &clilen);
        if (newsock < 0)
            error("Error accepting socket.");

        int buffersize = 1024;
        char* buffer = new char[buffersize];
        bzero(buffer,buffersize);

        err = read(newsock,buffer,buffersize-1);
        if (err < 0) {
            sendError(newsock, 500, "HTTP/1.1", "Unable to read from socket.");
            error("Error reading from accepted socket.");
        }
        parseRequest(newsock, buffer);

        delete[] buffer;
        close(newsock);
    }
}

int main(int argc, char* argv[])
{
    std::cout << "Starting server...\n";
    signal(SIGINT, signal_callback_handler);

    // read in settings file here...
    int configSize = -1;
    char* config = readFile("ws.conf", &configSize);
    if (config == NULL)
        error("Cound not find ws.conf file.");
    parseConfig(config);
    delete[] config;

	struct sockaddr_in serv_addr;

	bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;           // address family
	serv_addr.sin_port = htons(portNum);      // htons() sets the port # to network byte order
	serv_addr.sin_addr.s_addr = INADDR_ANY;   // supplies the IP address of the local machine

    // create a TCP socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error("Unable to create socket.");

    // bind the socket to the address
    socklen_t srvlen = sizeof(serv_addr);
    if (bind(sock, (struct sockaddr *) &serv_addr, srvlen) < 0)
        error("Unable to bind socket.");

    // wait for requests until a SIGINT
    loopMultithread();
}
