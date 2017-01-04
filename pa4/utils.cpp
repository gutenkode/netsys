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
