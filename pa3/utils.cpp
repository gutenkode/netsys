/*
IO functions that client and server may share.
To reduce code bloat.
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h> // read, write, close

using namespace std;

// Prints an error message and closes the program
void error(string err, int sock)
{
    cout << err << "\n";
    close(sock);
    exit(1);
}

// Tokenize a string
vector<string> split(const char *str, char c)
{
    vector<string> result;
    do {
        const char *begin = str;
        while(*str != c && *str)
            str++;

        result.push_back(string(begin, str));
    } while (0 != *str++);
    return result;
}

// Very basic encryption, just to show it works
void encrypt(char* data, int length, string password) {
    for (int i = 0; i < length; i++)
        data[i] += password[0];
}
void decrypt(char* data, int length, string password) {
    for (int i = 0; i < length; i++)
        data[i] -= password[0];
}

// Calculate checksum modded by a value for determining which servers to upload to
int getChecksumMod(char* data, int length, int mod) {
    int xVal = 0;
    for (int i = 0; i < length; i++)
        xVal += data[i];
    xVal %= mod;
    if (xVal < 0)
        xVal = 0;
    return xVal;
}

const char* listDir(string path, int* length)
{
    int LS_SIZE = 1024;
    *length = LS_SIZE;
    char* msg = new char[LS_SIZE];
    int ind = 0;
    string command = "/bin/ls "+path;
    FILE* fp = popen(command.c_str(), "r");
    while (ind != -1 && fgets(msg+ind, LS_SIZE-ind, fp) != NULL)
    {
        ind = strlen(msg); // terrible
        if (ind >= LS_SIZE-1) {
            return "Error: buffer for storing ls output is too small.";
        }
    }
    return msg;
}

// Reads in a file and returns a newly allocated char* of its contents
// The int pointer is set to the file's size
char* readFile(string filename, int* size)
{
    char* filedata = NULL;
    FILE *fp = fopen(filename.c_str(), "r");

	if (fp != NULL) {
		if (fseek(fp, 0L, SEEK_END) == 0) {
			// get the size of the file
			long bufsize = ftell(fp);
			if (bufsize == -1) { return NULL; }

			*size = sizeof(char) * bufsize;

			if (fseek(fp, 0L, SEEK_SET) != 0) { return NULL; }

            filedata = new char[*size];
			// actually read the file
			fread(filedata, sizeof(char), bufsize, fp);
			if (ferror(fp) != 0) {
                delete[] filedata;
				return NULL;
			}
		}
		fclose(fp);
	}
    return filedata;
}

void writeFile(string filename, char* filedata, int filesize)
{
    ofstream file;
    file.open(filename);
    file.write(filedata,filesize);
    file.close();
}
