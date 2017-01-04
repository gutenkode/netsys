An HTTP server.  Compile with g++, e.g. "g++ main.cpp".  Requires no arguments but uses a ws.conf file.

The main program listens on the specified port and creates a new thread for every request.  The thread parses the request and sends the appropriate error code or file.

The program can be terminated safely with Ctrl+C and will ensure all ports are closed before exiting.
