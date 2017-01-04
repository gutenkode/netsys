/*
Client should:
get user input GET, PUT, LIST, send to servers
receive data from servers, write files
upload files to servers
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

//#include <openssl/md5.h>
//#include "md5.h"

#include "utils.cpp"

using namespace std;

const int uploadTable[4][2] = {{1,2},{2,3},{3,4},{4,1}};
string globalEncryptPasswd;

// Receive a file from a server
char* getChunk(int sock, string in, int* size)
{
    int err;
    // tell the server we want a file
    err = write(sock, in.c_str(), in.length());
    if (err < 0)
        return NULL;//error("Error writing to socket, line "+to_string(__LINE__));

    // get the filesize
    int filesize = -1;
    err = read(sock,&filesize,sizeof(int));
    if (err < 0)
        return NULL;//error("Error reading from socket, line "+to_string(__LINE__));

    // if the server has the file, receive it
    if (filesize != -1)
    {
        char* buffer = new char[filesize];
        bzero(buffer,filesize);

        err = read(sock,buffer,filesize);
        if (err < 0)
            return NULL;//error("Error reading from socket, line "+to_string(__LINE__));

        decrypt(buffer, filesize, globalEncryptPasswd);

        *size = filesize;
        return buffer;
    }
    else
        return NULL;
}
// Will check all servers for a specific chunk and return the first one found
// Returns null if no chunk was found
char* getChunkInd(int* socks, int cind, string in, int* size)
{
    in = in+"."+to_string(cind);
    for (int i = 0; i < 4; i++) {
        int s = -1;
        char* c = getChunk(socks[i],in, &s);
        if (c != NULL) {
            *size = s;
            return c;
        }
    }
    cout << "Failed to find chunk: " << cind << " for file " << in << "\n";
    return NULL;
}

// Send a file to a server
void sendChunk(int sock, int chunknum, string in, char* chunk, int chunksize)
{
    int err;
    // tell the server to receive a file and its name, size, and chunk index
    string msg = "PUT "+in.substr(4)+"\\"+to_string(chunksize)+"\\"+to_string(chunknum); // redundant
    err = write(sock, msg.c_str(), msg.length());
    if (err >= 0)
    {    //error("Error writing to socket, line "+to_string(__LINE__));
        usleep(10000);
        // send the file
        err = write(sock, chunk, chunksize);
        if (err < 0)
            error("Error writing to socket, line "+to_string(__LINE__),sock);
    }
}

void loop(int socks[])
{
    int err;
    string in;
	do
	{
		printf("> ");
        in = "";
		getline(cin, in);

		if (in.substr(0,4) == "GET ")
		{
            int* sizes = new int[4];
            char* c1 = NULL;
            char* c2 = NULL;
            char* c3 = NULL;
            char* c4 = NULL;

            // try to get a copy of all four chunks
            bool failed = false;
            int ind = 0;
            while (!failed && ind < 4) {
                int csize = -1;
                char* chunk = getChunkInd(socks,ind+1,in,&csize);
                if (chunk != NULL) {
                    sizes[ind] = csize;
                    switch (ind) {
                        case 0: c1 = chunk; break;
                        case 1: c2 = chunk; break;
                        case 2: c3 = chunk; break;
                        case 3: c4 = chunk; break;
                    }
                } else
                    failed = true;
                ind++;
            }

            if (!failed)
            {
                // allocate file
                int totalSize = sizes[0]+sizes[1]+sizes[2]+sizes[3];
                char* file = new char[totalSize];
                memcpy(file, c1, sizes[0]);
                memcpy(file+sizes[0], c2, sizes[1]);
                memcpy(file+sizes[0]+sizes[1], c3, sizes[2]);
                memcpy(file+sizes[0]+sizes[1]+sizes[2], c4, sizes[3]);
                // write assembled data
                writeFile(in.substr(4)+".recv",file,totalSize);
                delete[] file;
                cout << "Wrote file " << in.substr(4) << ".recv\n";
            }
            else
                cout << "File is incomplete or missing.\n";

            if (c1 != NULL)
                delete[] c1;
            if (c2 != NULL)
                delete[] c2;
            if (c3 != NULL)
                delete[] c3;
            if (c4 != NULL)
                delete[] c4;
		}
		else if (in.substr(0,4) == "PUT ")
		{
            cout << "Reading file '" << in.substr(4) << "'\n";
            int filesize = -1;
            char* file = readFile(in.substr(4), &filesize);
            encrypt(file, filesize, globalEncryptPasswd);
            if (file != NULL)
            {
                int xVal = getChecksumMod(file, filesize, 4);
                cout << "Checksum result: " << xVal << "\nSending file...\n";

                // each chunk is a fourth of the file
                // last chunk might be uneven, whatever is left after 3/4ths are used
                int* chunksize = new int[4];
                chunksize[0] = filesize/4;
                chunksize[1] = chunksize[0];
                chunksize[2] = chunksize[0];
                chunksize[3] = filesize-(chunksize[0]*3);
                //cout << "CSize: " << chunksize[0] << " Last: " << chunksize[3] << "\n";

                for (int i = 0; i < 4; i++) {
                    int cind = uploadTable[(i+xVal)%4][0]; // index of the chunk to upload to server i
                    sendChunk(socks[i], cind, in, file+(cind-1)*chunksize[0], chunksize[cind-1]);
                    usleep(10000);
                    cind = uploadTable[(i+xVal)%4][1];
                    sendChunk(socks[i], cind, in, file+(cind-1)*chunksize[0], chunksize[cind-1]);
                }

                delete[] file;
            }
            else
                cout << "No such file.\n";
		}
		else if (in == "LIST")
		{
            string msg = "LIST";
            map<string,int> storedFiles;
            for (int i = 0; i < 4; i++)
            {
                err = write(socks[i], msg.c_str(), msg.length());
                if (err >= 0)
                {
                    int buffersize = 1024;
                    char* buffer = new char[buffersize];
                    bzero(buffer,buffersize);

                    err = read(socks[i],buffer,buffersize);
                    if (err < 0)
                        error("Error reading from socket, line "+to_string(__LINE__), socks[i]);

                    for (string s : split(buffer,'\n')) {
                        if (!s.empty()) {
                            string filename = s.substr(0,s.length()-2);
                            int pieceInd = stoi(s.substr(s.length()-1));
                            if (storedFiles[filename] == 0) {
                                storedFiles[filename] = 0;
                            }
                            storedFiles[filename] += 0x1 << (pieceInd-1);
                        }
                    }
                    delete[] buffer;
                }
            }
            for(map<string,int>::iterator iter = storedFiles.begin(); iter != storedFiles.end(); ++iter)
            {
                string k = iter->first;
                int v = iter->second;
                cout << k;
                if (v != 0x1E)
                    cout << " [incomplete]";
                cout << "\n";
            }
		}
        else if (in == "EXIT")
        {
            string msg = "EXIT";
            for (int i = 0; i < 4; i++) {
                err = write(socks[i], msg.c_str(), msg.length());
                //if (err < 0)
                //    error("Error writing to socket, line "+to_string(__LINE__));
            }
        }
        else
            cout << "No such command, '" << in << "'\n";
	}
	while(in != "EXIT");
	printf("Client exiting...\n");
}

int login(string hostname, int portnum, string username, string password) {
    int sock;//, portnum;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    //portnum = 9001;
    //const char* hostname = "localhost";

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error("Error opening socket, line "+to_string(__LINE__), sock);
    server = gethostbyname(hostname.c_str());
    if (server == NULL) {
        error("No such host, line "+to_string(__LINE__), sock);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(portnum);

    int err = connect(sock,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (err < 0) {
        cout << "Failed to connect to server " << hostname << ":" << portnum << "\n";//error("Error connecting, line "+to_string(__LINE__));
        return -1;
    }

    // send username
    err = write(sock, username.c_str(), username.length());
    if (err < 0)
        error("Error writing to socket, line "+to_string(__LINE__), sock);
    // check server response to username
    int result = -1;
    err = read(sock,&result,sizeof(int));
    if (err < 0)
        error("Error reading from socket, line "+to_string(__LINE__), sock);
    if (result != 1)
        error("Server refused username.", sock);
    // send password
    err = write(sock, password.c_str(), password.length());
    if (err < 0)
        error("Error writing to socket, line "+to_string(__LINE__), sock);
    // check server response to password
    result = -1;
    err = read(sock,&result,sizeof(int));
    if (err < 0)
        error("Error reading from socket, line "+to_string(__LINE__), sock);
    if (result != 1)
        error("Server refused password.", sock);

    return sock;
}

struct configData {
    static const int NUM_SERVERS = 4;
    string IPs[NUM_SERVERS], ports[NUM_SERVERS];
    string username, password;
};
configData parseConfig()
{
    int filesize = -1;
    char* file = readFile("dfc.conf", &filesize);
    configData cfg;
    int serverInd = 0;

    for (string str : split(file,'\n')) {
        if (str == "") {} else
        if (str.substr(0,7) == "Server ")
        {
            if (serverInd >= cfg.NUM_SERVERS)
                error("Config file lists extra servers beyond supported number.", -1);
            str = str.substr(7);
            vector<string> tok = split(str.c_str(),' ');
            vector<string> addr = split(tok[1].c_str(),':');
            cfg.IPs[serverInd] = addr[0];
            cfg.ports[serverInd] = addr[1];
            serverInd++;
        }
        else if (str.substr(0,10) == "Username: ")
        {
            cfg.username = str.substr(10);
        }
        else if (str.substr(0,10) == "Password: ")
        {
            cfg.password = str.substr(10);
        }
        else
            cout << "Invalid config line: '" << str << "'\n";
    }
    delete[] file;
    return cfg;
}

int* globalSocks;
void signal_callback_handler(int signum)
{
   cout << "\nCaught signal " << signum << "\n";
   close(globalSocks[0]);
   close(globalSocks[1]);
   close(globalSocks[2]);
   close(globalSocks[3]);
   exit(signum);
}

int main(int argc, char* argv[])
{
    cout << "Starting client...\n";

    configData cfg = parseConfig();

    int socks[4];
    socks[0] = login(cfg.IPs[0], stoi(cfg.ports[0]), cfg.username, cfg.password);
    socks[1] = login(cfg.IPs[1], stoi(cfg.ports[1]), cfg.username, cfg.password);
    socks[2] = login(cfg.IPs[2], stoi(cfg.ports[2]), cfg.username, cfg.password);
    socks[3] = login(cfg.IPs[3], stoi(cfg.ports[3]), cfg.username, cfg.password);

    globalSocks = socks;
    globalEncryptPasswd = cfg.password;

    loop(socks);

    close(socks[0]);
    close(socks[1]);
    close(socks[2]);
    close(socks[3]);
}
