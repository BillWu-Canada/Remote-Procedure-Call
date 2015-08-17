#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include "helper_code.h"
#include "binder.h"
using namespace std;

int roundRobinIndex = 0;
vector<string> servervector;
vector<string> allservers;
map<string, int> servers;
vector<entry> database;
int socket_fd;     // file descriptor of socket
fd_set master;    // master file descriptor list

int sendall(int s, const char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int binderInit() {
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number
    int socket_fd_accept;    // socket for accept
    socklen_t server_len;
    socklen_t client_len;
    int send_recv_checker;
    string receiving_buffer;
    char machine_name[200];    // store the machine name
    unsigned char message_length[4];
    unsigned char message_code[4];
    int message_length_int;
    int message_code_int;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    // create a socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        cerr << "open socket wrong in server" << endl;
        exit(1);
    }
    // clear the server_addr before use
    memset(&server_addr, 0, sizeof server_addr);
    // set elements in server_addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = 0;
    // bind the socket with the port
    int bind_check = bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bind_check < 0)
    {
        cerr << "error on binding on server" << endl;
        exit(1);
    }
    // get the machine name
    gethostname(machine_name, 99);
    server_len = sizeof(server_addr);
    getsockname(socket_fd, (struct sockaddr *)&server_addr, &server_len);
    cout << "BINDER_ADDRESS: " << machine_name << endl;
    cout << "BINDER_PORT: " << ntohs(server_addr.sin_port) << endl;
    cout << endl;
    // listen on port for maximum 5 clients and 5 servers
    if (listen(socket_fd, 10) == -1)
    {
        cerr << "error at listen on server" << endl;
        exit(1);
    }
    // add the listener to the master set
    FD_SET(socket_fd, &master);
    // keep track of the biggest file descriptor
    fdmax = socket_fd; // so far, it's this one
    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        // run through the existing connections looking for data to read
        for(int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == socket_fd) {
                    // handle new connections
                    client_len = sizeof(client_addr);
                    socket_fd_accept = accept(socket_fd, (struct sockaddr *) &client_addr, &client_len);
                    if (socket_fd_accept == -1) {
                        perror("accept");
                        } else {
                        FD_SET(socket_fd_accept, &master); // add to master set
                        if (socket_fd_accept > fdmax) {    // keep track of the max
                            fdmax = socket_fd_accept;
                        }
                        //cout << "client accepted" << endl;
                    }
                    } else {
                    // handle data from a client
                    if ((send_recv_checker = recv(i, message_length, sizeof message_length, 0)) <= 0) {
                        // got error or connection closed by client
                        if (send_recv_checker == 0) {
                            // connection closed
                            removeServer(i);
                            //printf("selectserver: socket %d hung upn", i);
                            //cout << "select_server" << endl;
                            } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        } else {
                        // we got some data from a client
                        // get the length of the string
                        message_length_int = (message_length[0] << 24) | (message_length[1] << 16) | (message_length[2] << 8) | message_length[3];
                        //cout << "length: " << message_length_int << endl;
                        send_recv_checker = recv(i, message_code, sizeof message_code, 0);
                        message_code_int = (message_code[0] << 24) | (message_code[1] << 16) | (message_code[2] << 8) | message_code[3];
                        //cout << "message code: " << message_code_int << endl;
                        // declare a message_received buffer
                        char message_receive[message_length_int];
                        //print out the message received
                        send_recv_checker = recv(i, message_receive, sizeof(message_receive), 0);
                        //cout << "message itself: "<< message_receive << endl;
                        if (binderProcessMessage(i, message_code_int, message_receive, message_length_int) == TERMINATE) {
                            exit(0);
                        }
                    }
                }
            }
        }
    }
}

int binderProcessMessage(int socket, int messageCode, char* message_receive, int message_length_int) {
    if (messageCode == REGISTER) { //register
        int sizes[3];
        int size_index = 0;
        for (int i = 0; i < message_length_int; i++) {
            if (message_receive[i] == ' ') {
                sizes[size_index] = i;
                size_index++;
            }
        }
        char servername[sizes[0]];
        memcpy(servername, message_receive, sizes[0]);
        //cout << servername << endl;
        int port;
        memcpy( &port, &message_receive[sizes[0]+1], sizes[1] - sizes[0]);
        //cout << port << endl;
        char name[sizes[2] - sizes[1]];
        memcpy( name, &message_receive[sizes[1]+1], sizes[2] - sizes[1]);
        //cout << name << endl;
        int args_length = (message_length_int - sizes[2])/sizeof(int);
        int args[args_length];
        for (int i = 0; i < args_length; i++) {
            memcpy( &args[i], message_receive + sizes[2] + 1 + i * sizeof(int), sizeof(int));
            //cout << "args" << i << " " << args[i] << endl;
        }
        //add server
        stringstream ss;
        ss << string(servername) << ' ' << port;
        addServer(ss.str(), socket);
        int result = binderAddVector(string(name), args, args_length, string(servername), port);
        //send result
        uint32_t server_result = htonl(result);
        int send_checker = send(socket, &server_result, sizeof(uint32_t), 0);
    } else if (messageCode == LOC_REQUEST) {
        int size;
        for (int i = 0; i < message_length_int; i++) {
            if (message_receive[i] == ' ') {
                size = i;
                break;
            }
        }
        //read name
        char name[size];
        memcpy( name, message_receive, size);
        //cout << name << endl;
        //read length
        int args_length = (message_length_int - size)/sizeof(int);
        int args[args_length];
        for (int i = 0; i < args_length; i++) {
            memcpy( &args[i], message_receive + size + 1 + i * sizeof(int), sizeof(int));
            //cout << args[i] << endl;
        }
        //find server
        string returnmessage = findServer(string(name), args, args_length);
        if (returnmessage.length() == 0) { // fail
            int msize = htonl(sizeof(int));
            send(socket, &msize, sizeof(uint32_t), 0); // send 4
            int mcode = htonl(LOC_FAILURE);
            send(socket, &mcode, sizeof(uint32_t), 0); // send fail
            int mreason = htonl(LOC_NO_SERVER);
            send(socket, &mreason, sizeof(uint32_t), 0); // send not found
            } else { //success
            const char *main_message = returnmessage.c_str();
            int msize = htonl(returnmessage.length() + 1);
            send(socket, &msize, sizeof(uint32_t), 0); // send length
            int mcode = htonl(LOC_SUCCESS);
            send(socket, &mcode, sizeof(uint32_t), 0); // send success
            sendall(socket, main_message, &msize); // send string
        }
    } else if (messageCode == TERMINATE) {
        binderTerminate();
        return TERMINATE;
    } else if (messageCode == SERVER_REGISTER) {
        //cout << "message recieved" << message_receive << endl;
        allservers.push_back(string(message_receive));
    } else if (messageCode == CACHE_REQUEST) {
        int size;
        for (int i = 0; i < message_length_int; i++) {
            if (message_receive[i] == ' ') {
                size = i;
                break;
            }
        }
        //read name
        char name[size];
        memcpy( name, message_receive, size);
        //cout << name << endl;
        //read length
        int args_length = (message_length_int - size)/sizeof(int);
        int args[args_length];
        for (int i = 0; i < args_length; i++) {
            memcpy( &args[i], message_receive + size + 1 + i * sizeof(int), sizeof(int));
            //cout << args[i] << endl;
        }
        //find server
        string returnmessage = findAllServer(string(name), args, args_length);
        if (returnmessage.length() == 0) { // fail
            int msize = htonl(sizeof(int));
            send(socket, &msize, sizeof(uint32_t), 0); // send 4
            int mcode = htonl(CACHE_FAILURE);
            send(socket, &mcode, sizeof(uint32_t), 0); // send fail
            int mreason = htonl(CACHE_NO_SERVER);
            send(socket, &mreason, sizeof(uint32_t), 0); // send not found
            } else { //success
            const char *main_message = returnmessage.c_str();
            int msize = htonl(returnmessage.length() + 1);
            send(socket, &msize, sizeof(uint32_t), 0); // send length
            int mcode = htonl(CACHE_SUCCESS);
            send(socket, &mcode, sizeof(uint32_t), 0); // send success
            sendall(socket, main_message, &msize); // send string
        }
    }
}

int sameFunction(int* arg, int arg_length, int* arg2, int arg2_length) {
    if (arg_length != arg2_length) {
        return 0;
    }
    for (int i = 0; i < arg_length; i++) {
        int array_length = arg[i] & 0xFFFF;
        int rest_of_arg = arg[i] & 0xFFFF0000;
        int array2_length = arg2[i] & 0xFFFF;
        int rest_of_arg2 = arg2[i] & 0xFFFF0000;
        if (array_length > 0 && array2_length == 0) {
            //cout << "different lengths " << arg[i] << " " << arg2[i] << endl;
            return 0;
        }
    }
    return 1;
}

int binderAddVector(string name, int* args, int args_length, string server, int port) {
    for(vector<entry>::iterator it = database.begin(); it != database.end(); ++it) {
        if ((it->name.compare(name) == 0) && it->args_length == args_length) {
            if (sameFunction(it->args, it->args_length, args, args_length)) {
                //update new args
                free(it->args);
                it->args = (int*) malloc(args_length*sizeof(int));
                for (int i = 0; i < args_length; i++) {
                    it->args[i] = args[i];
                }
                stringstream ss;
                ss << server << ' ' << port;
                vector<string>::iterator it2 = find(it->servers.begin(), it->servers.end(), ss.str());
                if (it2 == it->servers.end()) {
                    it->servers.push_back(ss.str());
                    return REGISTER_SUCCESS; //not in database
                }
                return REGISTER_WARNING; //already in database
            }
        }
    }
    //if not in database
    entry newEntry;
    newEntry.name = name;
    newEntry.args = (int*) malloc(args_length*sizeof(int));
    for (int i = 0; i < args_length; i++) {
        newEntry.args[i] = args[i];
    }
    newEntry.args_length = args_length;
    vector<string> temp;
    stringstream s;
    s << server << ' ' << port;
    temp.push_back(s.str());
    newEntry.servers = temp;
    database.push_back(newEntry);
    return REGISTER_SUCCESS;
}

void addServer(string name, int socket) {
    //cout << "adding server " << name  << endl;
    map<string, int>::const_iterator it = servers.find(name);
    //cout << "b4 add" << endl;
    if (it == servers.end()) {
        servers[name] = socket;
        servervector.push_back(name);
        //cout << "added" << endl;
    }
    //cout<< "server0 " << servers[name] <<endl;
}

string findServer(string name, int* args, int args_length) {
    //cout << "start find" << endl;
    for(vector<entry>::iterator it = database.begin(); it != database.end(); ++it) {
        //cout << it->name << " " << name << endl;
        if ((it->name.compare(name) == 0) && it->args_length == args_length) {
            //cout << "same name and args" << endl;
            if (sameFunction(it->args, it->args_length, args, args_length)) {
                //cout << "same funct" << endl;
                if (roundRobinIndex > (servervector.size() - 1)) {
                     roundRobinIndex = roundRobinIndex % servervector.size();
                }
                int lastRoundRobinIndex = roundRobinIndex;
                while (servervector.size() > 0 && it->servers.size() > 0) {
                    string curserver = servervector.at(roundRobinIndex);
                    vector<string>::const_iterator it2 = find(it->servers.begin(), it->servers.end(), curserver);
                    roundRobinIndex = (roundRobinIndex + 1) % servervector.size();
                    if (it2 != it->servers.end()) { //has to hit here eventually since the server exists
                        //cout << curserver << endl;
                        return curserver; //found;
                    }
                    if (lastRoundRobinIndex == roundRobinIndex) {
                        return "";
                    }
                }
            }
        }
    }
    return "";
}

string findAllServer(string name, int* args, int args_length) {
    //cout << "start find" << endl;
    stringstream s;
    for(vector<entry>::iterator it = database.begin(); it != database.end(); ++it) {
        //cout << it->name << " " << name << endl;
        if ((it->name.compare(name) == 0) && it->args_length == args_length) {
            //cout << "same name and args" << endl;
            if (sameFunction(it->args, it->args_length, args, args_length)) {
                for(vector<string>::iterator it2 = it->servers.begin(); it2 != it->servers.end(); ++it2) {
                    if (s.str().length() == 0) {
                         s << (*it2);
                    } else {
                         s << ' ' << (*it2);
                    }
                }
                break;
            }
        }
    }
    //cout << s.str() << endl;
    return s.str();
}

int binderTerminate() {

    for(vector<string>::iterator it = allservers.begin(); it != allservers.end(); ++it) {
    //for(map<string,int>::iterator it = servers.begin(); it != servers.end(); ++it) {
            //socket2 = it->second;

 // parse received message into server_id and port
   int sizes[1];
   int size_index = 0;
                      
  for (int i = 0; i < (*it).length(); i++) {
      if ((*it).at(i) == ' ') {
            sizes[size_index] = i;
            size_index++;
      }
  }

  int ip_length = sizes[0];
  int port_length = (*it).length() - ip_length;

  char server_identifier[ip_length + 1];
  char port[port_length];

  server_identifier[ip_length] = '\0';

  memcpy(server_identifier, (*it).c_str(), ip_length);

  memcpy(port, (*it).c_str()+ip_length+1, port_length);

  //cout << "recv server_identifier: " << server_identifier << endl;
  //cout << "recv port: " << port << endl;

  // ===================================================================
  // connect to the specific server
  
  struct hostent* host_server_2;
  struct addrinfo hints_2;
  struct addrinfo *server_addr_2, *try_temp_2;
  int socket_from_client_to_server;

  host_server_2 = gethostbyname(server_identifier);
  if (host_server_2 == NULL)
  {
    cerr<< "get server host error" << endl;
    return -17;
  }

  int port_number_2 = atoi(port);

   memset(&hints_2, 0, sizeof hints_2); // make sure the struct is empty
   hints_2.ai_family = AF_INET;     // IPv4
   hints_2.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints_2.ai_flags = AI_PASSIVE;     // fill in my IP for me


   int status_checker = 0;
   if ((status_checker = getaddrinfo(host_server_2->h_name, port, &hints_2, &server_addr_2)) != 0) {
      cerr << "getaddrinfo error on client" << endl;
      exit(1);
   }
 
   for(try_temp_2 = server_addr_2; try_temp_2 != NULL; try_temp_2 = try_temp_2->ai_next) {
        if ((socket_from_client_to_server = socket(try_temp_2->ai_family, try_temp_2->ai_socktype,
                try_temp_2->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(socket_from_client_to_server, try_temp_2->ai_addr, try_temp_2->ai_addrlen) == -1) {
            close(socket_from_client_to_server);
            perror("client: connect");
            continue;
        }

        break;
    }

    // connect to server success
    // start to send execute message: [EXECUTE, name, argtypes, args]


            int msize = htonl(sizeof(int));
            send(socket_from_client_to_server, &msize, sizeof(uint32_t), 0); // send 4
            int mcode = htonl(TERMINATE);
            send(socket_from_client_to_server, &mcode, sizeof(uint32_t), 0); // send terminate
            send(socket_from_client_to_server, &mcode, sizeof(uint32_t), 0); // send terminate

            //close(socket2);
            //FD_CLR(socket2, &master);
    }

    for(vector<entry>::iterator it = database.begin(); it != database.end(); ++it) {
        free(it->args);
    }
    close(socket_fd);
}

void removeServer(int socket) {
    int index = 0;
    string servername = "";

    //delete the server in map
    for(map<string,int>::iterator it = servers.begin(); it != servers.end(); ++it) {
        if (it->second == socket) {
            servername = it->first;
            servers.erase(it);
            break;
        }
        index++;
    }

    if (servername.compare("") == 0) {
        return; //not a server or server does not exist
    }

    //delete the server at servervector
    servervector.erase(servervector.begin() + index);

    //delete from each it
    for(vector<entry>::iterator it = database.begin(); it != database.end(); ++it) {
        vector<string>::iterator it2 = find(it->servers.begin(), it->servers.end(), servername);
        if (it2 != it->servers.end()) {
        it->servers.erase(it2);
        }
    }

    //delete server at allservers
    vector<string>::iterator it2 = find(allservers.begin(), allservers.end(), servername);
    if (it2 != allservers.end()) {
     allservers.erase(it2);
    }
}

int main() {
    int result = binderInit();
    //char* temp = "efawfwef";
    //binderProcessMessage(0, temp, 9, "fwafaew");
    //cout<<database.at(0).servers.at(0)<<endl;
    //binderAddVector(temp, 9, "efawfwg");
}

