#ifndef __binder_h__
#define __binder_h__

struct entry {
    string name;
    int* args;
    int args_length;
    vector<string> servers;
};

int binderProcessMessage(int socket, int messageCode, char* message_receive, int message_length_int);
int binderAddVector(string name, int* args, int args_length, string server, int port);
void addServer(string servername, int socket);
string findServer(string name, int* args, int args_length);
string findAllServer(string name, int* args, int args_length);
int binderTerminate();
void removeServer(int socket);

#endif
