Distributed fileserver with TCP.  A client will connect to four servers and send/receive files in parts.  Each server receives a fourth of a whole file, and the client handles distributing/assembling whole files.

Build with 'make', produces 'client' and 'server' programs.  Run client as 'client dfc.conf' and server as 'server <directory <port>', ex. 'server /DFS1 9001'.  Client uses GET, PUT, LIST, EXIT (capitalized) to talk to the server.

