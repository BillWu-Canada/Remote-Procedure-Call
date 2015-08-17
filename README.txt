group members information:

NAME          RESPONSIBILITIES
------------------------------------
Alex Luk      binder; rpcCacheCall
Yinwen Wu     rpcInit; rpcCall; rpcRegister; rpcExecute; rpcTerminate



How to compile and run RPC system under University of Waterloo's linux.student.cs environment:

(1) type "make" to compile
(2) g++ -L. client.o -lrpc -o client or the exact command specified in your README for compiling our client
(3) g++ -L. server functions.o server function skels.o server.o -lrpc -o server or the exact command specified in your README for compiling our server
(4) start the binder by typing ./binder
(5) Manually set the BINDER ADDRESS and BINDER PORT environment variables on the client and server machines. (Use setenv if using C shell)
(6) typing ./server and ./client to run our server(s) and client(s). Note that the binder, client and server may be running on different machines.



dependencies:
binder: binder.cc binder.h
library: rpc.cc rpc.h server_database.cc server_database.h helper_code.h


