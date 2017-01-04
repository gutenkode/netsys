A UDP client-server model fileserver. Files can be stored and received by the server.

The server.c and client.c files were adapted from the example files provided, build normally with make.
The utils.c file contains shared code for sending/receiving files that both the client and server use.

Files are split into 2KB chunks for transfer, as bigger chunks often failed.
No extensive reliability has been implemented - packets are never resent, and ordering is not checked.
The client will, however, detect if it has not received all the required packets within a timeout and state that the transfer failed.

The programs will recover from most failure states and continue running, such as when the ls command returns more data than the buffer size.
