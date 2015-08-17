#include "rpc.h"
#include "helper_code.h"
#include "server_database.h"
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
#include <sstream>
#include <vector>
#include <sstream>
#include <algorithm>
using namespace std;

struct entry {
    string name;
    int* args;
    int args_length;
    vector<string> servers;
};

vector<entry> database;

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

void rpcAddVector(string name, int* args, int args_length, string server, int port) {
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
                    //return REGISTER_SUCCESS; //not in database
                }
                //return REGISTER_WARNING; //already in database
                return;
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
    //cout << "push bakc" << s.str() << endl;
    newEntry.servers = temp;
    database.push_back(newEntry);
}

int currentIndex = 0;
string findServer(string name, int* args, int args_length) {
    //cout << "start find" << endl;
    for(vector<entry>::iterator it = database.begin(); it != database.end(); ++it) {
        //cout << it->name << " " << name << endl;
        if ((it->name.compare(name) == 0) && it->args_length == args_length) {
            //cout << "same name and args" << endl;
            if (sameFunction(it->args, it->args_length, args, args_length)) {
                if (it->servers.size() == 0) {
                      return "";
                }
                return it->servers.at(currentIndex);
            }
        }
    }
    return "";
}

int findServerLength(string name, int* args, int args_length) {
    //cout << "start find" << endl;
    for(vector<entry>::iterator it = database.begin(); it != database.end(); ++it) {
        //cout << it->name << " " << name << endl;
        //cout <<
        if ((it->name.compare(name) == 0) && it->args_length == args_length) {
            //cout << "same name and args" << endl;
            if (sameFunction(it->args, it->args_length, args, args_length)) {
                return it->servers.size();
            }
        }
    }
    return 0;
}


// for rpcInit()
int socket_fd;     // file descriptor of socket
struct sockaddr_in server_addr;   // server address
int socket_fd_binder;

int socket_terminate;

int rpcInit() 
{
	// create a connection socket to be used for accepting connections form clients


   //int socket_fd_accept;    // socket for accept
   socklen_t server_len;
   socklen_t client_len;
   char machine_name[100];
   int PORT;

   //struct sockaddr_in client_addr;

    // create a socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0)
    {
    	 cerr << "open socket wrong in server" << endl;
         return -31;
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
        return -32;
    }

    // get the machine name
    gethostname(machine_name, 99);

    server_len = sizeof(server_addr);
    getsockname(socket_fd, (struct sockaddr *)&server_addr, &server_len);

    PORT = ntohs(server_addr.sin_port);

    //cout << "SERVER_ADDRESS: " << machine_name << endl;
    //cout << "SERVER_PORT: " << PORT << endl;


    // listen on port for maximum 5 clients
   if (listen(socket_fd, 5) == -1)
   {
       cerr << "error at listen on server" << endl;
       return -33;
   }

   //cout << "socket fd for listen: " << socket_fd << endl;


   // connect to the binder
  struct addrinfo hints;
  struct addrinfo *binder, *try_temp;
  struct hostent* host_server;
  int port_number;
  
  //cout<<"before binder get"<<endl;

   char* binder_addr = getenv("BINDER_ADDRESS");
   char* binder_port = getenv("BINDER_PORT");

   //cout<<"after binder get"<<endl;

   if (binder_addr == NULL || binder_port == NULL)
   {
      cerr << "can't find a binder, exit..." << endl;
      return -34;
   }

   host_server = gethostbyname(binder_addr);
   if (host_server == NULL)
   {
       cerr << "can't get the host_server on client" << endl;
       return -35;
   }

   port_number = atoi(binder_port);


   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_INET;     // IPv4
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

   if ( getaddrinfo(host_server->h_name, binder_port, &hints, &binder) != 0) {
      cerr << "getaddrinfo error on client" << endl;
      return -36;
   }

 
   for(try_temp = binder; try_temp != NULL; try_temp = try_temp->ai_next) {
        if ((socket_fd_binder = socket(try_temp->ai_family, try_temp->ai_socktype,
                try_temp->ai_protocol)) == -1) {
            perror("client: socket");
            return SOCKET_CREATE_FAIL;
            // continue;
        }

        if (connect(socket_fd_binder, try_temp->ai_addr, try_temp->ai_addrlen) == -1) {
            close(socket_fd_binder);
            perror("client: connect");
            return SOCKET_CREATE_FAIL;
            // continue;
        }

        break;
    }


    // send server address and port to binder
    

    stringstream ss;
    ss << machine_name << ' ' << PORT;
    

    string temp = ss.str();
    const char* ip_and_port = temp.c_str(); 

    //cout << "ip_and_port: " << ip_and_port << endl;

    int size_of_ss = strlen(ip_and_port) + 1;
    //cout << "size_of_ss: " << size_of_ss << endl;

    char main_message[size_of_ss];

    memcpy(main_message, ip_and_port, size_of_ss);

    //cout << "main_message: " << "|" <<main_message << "|" << endl;
    
    // send message to binder
    // send message length

    int send_checker;

    uint32_t MESSAGE_LENGTH_byte = htonl(size_of_ss);
    send_checker = send(socket_fd_binder, &MESSAGE_LENGTH_byte, sizeof(uint32_t), 0);
    if (send_checker < 0)
    {
       cerr << "can't find the binder, first send error" << endl;
       return -10;
    }


    int MESSAGE_TYPE = SERVER_REGISTER;

    uint32_t MESSAGE_TYPE_byte = htonl(MESSAGE_TYPE);
    send_checker =  send(socket_fd_binder, &MESSAGE_TYPE_byte, sizeof(uint32_t), 0);
    if (send_checker < 0)
    {
       cerr << "can't find the binder, second send error" << endl;
       return -11;
    }

    send_checker = sendall(socket_fd_binder, main_message, &size_of_ss);
    if (send_checker < 0)
    {
       cerr << "can't find the binder, third send error" << endl;
       return -12;
    }

    return 0; 

}


int rpcCall(char* name, int* argTypes, void** args)
{
	// talk to the binder at first

    // connect to the server if there is a avaliable one
    struct hostent* host_server;
    struct addrinfo *server_addr, *try_temp;
    int socket_from_client_to_binder;
    struct addrinfo hints;
    int status_checker;

    int MESSAGE_LENGTH;
    int MESSAGE_TYPE;

    unsigned char message_length[4];
    unsigned char message_code[4];
    int message_length_int;
    int message_code_int;

    char* SA = getenv("BINDER_ADDRESS");
    char* PO = getenv("BINDER_PORT");

    if (SA==NULL || PO==NULL)
    {
       cerr << "client can't locate the binder" << endl;
       return -37;
    }


   host_server = gethostbyname(SA);
   if (host_server == NULL)
   {
       cerr << "can't get the host_server on client" << endl;
       exit(1);
   }

   int port_number = atoi(PO);


   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_INET;     // IPv4
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

   if ((status_checker = getaddrinfo(host_server->h_name, PO, &hints, &server_addr)) != 0) {
      cerr << "getaddrinfo error on client" << endl;
      exit(1);
   }
 
   for(try_temp = server_addr; try_temp != NULL; try_temp = try_temp->ai_next) {
        if ((socket_from_client_to_binder = socket(try_temp->ai_family, try_temp->ai_socktype,
                try_temp->ai_protocol)) == -1) {
            perror("client: socket");
            return SOCKET_CREATE_FAIL;
            //continue;
        }

        if (connect(socket_from_client_to_binder, try_temp->ai_addr, try_temp->ai_addrlen) == -1) {
            close(socket_from_client_to_binder);
            perror("client: connect");
            return CONNECT_FAIL;
            //continue;
        }

        break;
    }

    //cout << "here" << endl;

    socket_terminate = socket_from_client_to_binder;

    // connection to binder successful
    // send [loc_request, name, argtypes]
    int send_checker = buffer_send(socket_from_client_to_binder, LOC_REQUEST, name, argTypes, NULL);
    if (send_checker != 0)
    {
       return send_checker;
    }

   
   // loc_request send complete
   // wait to recv loc_succuess or loc_failure message
   int recv_checker;
   
   recv_checker = recv(socket_from_client_to_binder, message_length, sizeof message_length, 0);
   if (recv_checker < 0)
   {
    cerr << "first recv error" << endl;
    return -13;
   }

   message_length_int = (message_length[0] << 24) | (message_length[1] << 16) | (message_length[2] << 8) | message_length[3];

   //cout << "recv message length: " << message_length_int << endl;


   recv_checker = recv(socket_from_client_to_binder, message_code, sizeof message_code, 0);
   if (recv_checker < 0)
   {
    cerr << "second recv error" << endl;
    return -14;
   }

   message_code_int = (message_code[0] << 24) | (message_code[1] << 16) | (message_code[2] << 8) | message_code[3];
   
   //cout << "message type recv: " << message_code_int << endl;

   char message_receive[message_length_int];

   recv_checker = recv(socket_from_client_to_binder, message_receive, sizeof(message_receive), 0);
   if (recv_checker < 0)
   {
    cerr << "third recv error" << endl;
    return -15;
   }

   //cout << "received message itself: "<< message_receive << endl;

   if (message_code_int == LOC_FAILURE)
   {
       unsigned char* message_receive_to_int = (unsigned char*)message_receive;

       int message_receive_int = (message_receive_to_int[0] << 24) | (message_receive_to_int[1] << 16) | (message_receive_to_int[2] << 8) | message_receive_to_int[3];
       //cout << "loc request fail: " << message_receive_int << endl;
       return message_receive_int;
   }

   // parse received message into server_id and port
   int sizes[1];
   int size_index = 0;
                      
  for (int i = 0; i < message_length_int; i++) {
      if (message_receive[i] == ' ') {
            sizes[size_index] = i;
            size_index++;
      }
  }

  int ip_length = sizes[0];
  int port_length = message_length_int - ip_length - 1;

  char server_identifier[ip_length + 1];
  char port[port_length];

  server_identifier[ip_length] = '\0';

  memcpy(server_identifier, message_receive, ip_length);

  memcpy(port, message_receive+ip_length+1, port_length);

  //cout << endl;
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



   if ((status_checker = getaddrinfo(host_server_2->h_name, port, &hints_2, &server_addr_2)) != 0) {
      cerr << "getaddrinfo error on client" << endl;
      exit(1);
   }
 
   for(try_temp_2 = server_addr_2; try_temp_2 != NULL; try_temp_2 = try_temp_2->ai_next) {
        if ((socket_from_client_to_server = socket(try_temp_2->ai_family, try_temp_2->ai_socktype,
                try_temp_2->ai_protocol)) == -1) {
            perror("client: socket");
            return SOCKET_CREATE_FAIL;
            //continue;
        }

        if (connect(socket_from_client_to_server, try_temp_2->ai_addr, try_temp_2->ai_addrlen) == -1) {
            close(socket_from_client_to_server);
            perror("client: connect");
            return CONNECT_FAIL;
            //continue;
        }

        break;
    }

    // connect to server success
    // start to send execute message: [EXECUTE, name, argtypes, args]
    send_checker = buffer_send(socket_from_client_to_server, EXECUTE, name, argTypes, args);
    if (send_checker != 0)
    {
       return send_checker;
    }

    // receive from server
   recv_checker = recv(socket_from_client_to_server, message_length, sizeof message_length, 0);
   if (recv_checker < 0)
   {
    cerr << "first recv error" << endl;
    return -13;
   }

   message_length_int = (message_length[0] << 24) | (message_length[1] << 16) | (message_length[2] << 8) | message_length[3];

   //cout << "recv message length: " << message_length_int << endl;


   recv_checker = recv(socket_from_client_to_server, message_code, sizeof message_code, 0);
   if (recv_checker < 0)
   {
    cerr << "second recv error" << endl;
    return -14;
   }

   message_code_int = (message_code[0] << 24) | (message_code[1] << 16) | (message_code[2] << 8) | message_code[3];
   
   //cout << "message type recv: " << message_code_int << endl;

   char recv_buffer[message_length_int];

   recv_checker = recv(socket_from_client_to_server, recv_buffer, sizeof(recv_buffer), 0);
   if (recv_checker < 0)
   {
    cerr << "third recv error" << endl;
    return -15;
   }

   //cout << "received message itself: "<< recv_buffer << endl;

   if (message_code_int == EXECUTE_FAILURE)
   {
       unsigned char* message_receive_to_int = (unsigned char*)recv_buffer;

       int message_receive_int = (message_receive_to_int[0] << 24) | (message_receive_to_int[1] << 16) | (message_receive_to_int[2] << 8) | message_receive_to_int[3];
       //cout << "execute fail from server, reasoncode: " << message_receive_int << endl;
       return message_receive_int;
   }

   // unpacket message
   int unpacket_checker = final_unpacket(recv_buffer, message_length_int, argTypes, args);

   //cout << "END OF RPCCALL, yeah !!" << endl;
   return 0;
    
}




int rpcCacheCall(char* name, int* argTypes, void** args) {
  // talk to the binder at first

    if (findServerLength(name, argTypes, args_size_checker(argTypes)) == 0) {
    // connect to the server if there is a avaliable one
    struct hostent* host_server;
    struct addrinfo *server_addr, *try_temp;
    int socket_from_client_to_binder;
    struct addrinfo hints;
    int status_checker;

    int MESSAGE_LENGTH;
    int MESSAGE_TYPE;

    unsigned char message_length[4];
    unsigned char message_code[4];
    int message_length_int;
    int message_code_int;

    char* SA = getenv("BINDER_ADDRESS");
    char* PO = getenv("BINDER_PORT");

    if (SA==NULL || PO==NULL)
    {
       cerr << "client can't locate the binder" << endl;
       return -37;
    }


   host_server = gethostbyname(SA);
   if (host_server == NULL)
   {
       cerr << "can't get the host_server on client" << endl;
       exit(1);
   }

   int port_number = atoi(PO);


   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_INET;     // IPv4
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

   if ((status_checker = getaddrinfo(host_server->h_name, PO, &hints, &server_addr)) != 0) {
      cerr << "getaddrinfo error on client" << endl;
      exit(1);
   }
 
   for(try_temp = server_addr; try_temp != NULL; try_temp = try_temp->ai_next) {
        if ((socket_from_client_to_binder = socket(try_temp->ai_family, try_temp->ai_socktype,
                try_temp->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(socket_from_client_to_binder, try_temp->ai_addr, try_temp->ai_addrlen) == -1) {
            close(socket_from_client_to_binder);
            perror("client: connect");
            continue;
        }

        break;
    }

    //cout << "here" << endl;

    socket_terminate = socket_from_client_to_binder;

    // connection to binder successful
    // send [loc_request, name, argtypes]
    int send_checker = buffer_send(socket_from_client_to_binder, CACHE_REQUEST, name, argTypes, NULL);
    if (send_checker != 0)
    {
       return send_checker;
    }

   
   // loc_request send complete
   // wait to recv loc_succuess or loc_failure message
   int recv_checker;
   
   recv_checker = recv(socket_from_client_to_binder, message_length, sizeof message_length, 0);
   if (recv_checker < 0)
   {
    cerr << "first recv error" << endl;
    return -13;
   }

   message_length_int = (message_length[0] << 24) | (message_length[1] << 16) | (message_length[2] << 8) | message_length[3];

   //cout << "recv message length: " << message_length_int << endl;


   recv_checker = recv(socket_from_client_to_binder, message_code, sizeof message_code, 0);
   if (recv_checker < 0)
   {
    cerr << "second recv error" << endl;
    return -14;
   }

   message_code_int = (message_code[0] << 24) | (message_code[1] << 16) | (message_code[2] << 8) | message_code[3];
   
   //cout << "message type recv: " << message_code_int << endl;

   char message_receive[message_length_int];

   recv_checker = recv(socket_from_client_to_binder, message_receive, sizeof(message_receive), 0);
   if (recv_checker < 0)
   {
    cerr << "third recv error" << endl;
    return -15;
   }

   //cout << "received message itself: "<< message_receive << endl;

   if (message_code_int == CACHE_FAILURE)
   {
       unsigned char* message_receive_to_int = (unsigned char*)message_receive;

       int message_receive_int = (message_receive_to_int[0] << 24) | (message_receive_to_int[1] << 16) | (message_receive_to_int[2] << 8) | message_receive_to_int[3];
       //cout << "loc request fail: " << message_receive_int << endl;
       return message_receive_int;
   }

   // parse received message into server_id and port
   int ip_length = -1;
   int port_length = -1;
   int previous = 0;
   int total = 0;                  
  for (int i = 0; i <= message_length_int; i++) {
      //cout << i << endl;
      if (i == message_length_int) {
           port_length = i;
           char server_identifier[ip_length  - previous + 1];
           char port[port_length - ip_length];
        
           server_identifier[ip_length - previous] = '\0';
           port[port_length] = '\0';

           memcpy(server_identifier, message_receive + total, ip_length  - previous);

           memcpy(port, message_receive+total+ip_length+1, port_length - ip_length);

           rpcAddVector(string(name), argTypes, args_size_checker(argTypes), string(server_identifier), atoi(port));
           //cout << "recv server_identifier: " << server_identifier << endl;
           //cout << "recv port: " << port << endl;
           break;
      }
      if (message_receive[i] == ' ') {
            if (ip_length == -1) {
                ip_length = i;
                //cout << i << endl;
                continue;
            }
            if (port_length == -1) {
                port_length = i;

                char server_identifier[ip_length - previous + 1];
                char port[port_length - ip_length + 1];
                
                server_identifier[ip_length - previous] = '\0';
                port[port_length - ip_length] = '\0';

                memcpy(server_identifier, message_receive + total, ip_length - previous);

                memcpy(port, message_receive+total+ip_length+1, port_length - ip_length);

                //cout << "recv server_identifier: " << server_identifier << endl;
                //cout << "recv port: " << port << endl;
                rpcAddVector(string(name), argTypes, args_size_checker(argTypes), string(server_identifier), atoi(port));

                previous = port_length + 1;
                ip_length = -1;
                port_length = -1;
                total += port_length + 1;
                if (i != message_length_int - 1) {
                    i++;
                }
            }
      }
  }
  }


  // ===================================================================
  // connect to the specific server
  int send_checker;
  int recv_checker;
  int currentIndex = 0;
  int serverslength = findServerLength(name, argTypes, args_size_checker(argTypes));
  //cout << serverslength << endl;
  while (currentIndex < serverslength) {
   string serverret = findServer(name, argTypes, args_size_checker(argTypes));
   //cout << serverret << endl;
   int message_length_int = serverret.length();
   const char* message_receive = serverret.c_str();

   // parse received message into server_id and port
   int sizes[1];
   int size_index = 0;
                      
  for (int i = 0; i < message_length_int; i++) {
      if (message_receive[i] == ' ') {
            sizes[size_index] = i;
            size_index++;
      }
  }

  int ip_length = sizes[0];
  int port_length = message_length_int - ip_length - 1;

  char server_identifier[ip_length + 1];
  char port[port_length];

  server_identifier[ip_length] = '\0';

  memcpy(server_identifier, message_receive, ip_length);

  memcpy(port, message_receive+ip_length+1, port_length);

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
  stringstream ss;
  ss << port_number_2;
  //cout << port_number_2 << endl;

   memset(&hints_2, 0, sizeof hints_2); // make sure the struct is empty
   hints_2.ai_family = AF_INET;     // IPv4
   hints_2.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints_2.ai_flags = AI_PASSIVE;     // fill in my IP for me


   int status_checker;
   if ((status_checker = getaddrinfo(host_server_2->h_name, ss.str().c_str(), &hints_2, &server_addr_2)) != 0) {
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
    send_checker = buffer_send(socket_from_client_to_server, EXECUTE, name, argTypes, args);
    if (send_checker != 0)
    {
       currentIndex+=1;
       continue;
    }

    // receive from server
 char message_length[sizeof(int)];
char message_code[sizeof(int)];
   recv_checker = recv(socket_from_client_to_server, message_length, sizeof message_length, 0);
   if (recv_checker < 0)
   {
    cerr << "first recv error" << endl;
    return -13;
   }

   int message_length_int2 = (message_length[0] << 24) | (message_length[1] << 16) | (message_length[2] << 8) | message_length[3];

   //cout << "recv message length: " << message_length_int << endl;


   recv_checker = recv(socket_from_client_to_server, message_code, sizeof message_code, 0);
   if (recv_checker < 0)
   {
    cerr << "second recv error" << endl;
    return -14;
   }

   int message_code_int2 = (message_code[0] << 24) | (message_code[1] << 16) | (message_code[2] << 8) | message_code[3];
   
   //cout << "message type recv: " << message_code_int << endl;

   char recv_buffer[message_length_int2];

   recv_checker = recv(socket_from_client_to_server, recv_buffer, sizeof(recv_buffer), 0);
   if (recv_checker < 0)
   {
    cerr << "third recv error" << endl;
    return -15;
   }

   //cout << "received message itself: "<< recv_buffer << endl;

   if (message_code_int2 == EXECUTE_FAILURE)
   {
       unsigned char* message_receive_to_int = (unsigned char*)recv_buffer;

       int message_receive_int = (message_receive_to_int[0] << 24) | (message_receive_to_int[1] << 16) | (message_receive_to_int[2] << 8) | message_receive_to_int[3];
       //cout << "execute fail from server, reasoncode: " << message_receive_int << endl;
       currentIndex+=1;
       continue;
   }
   // unpacket message
   int unpacket_checker = final_unpacket(recv_buffer, message_length_int2, argTypes, args);
   break;
  }

  if (currentIndex > serverslength) {
       rpcCacheCall(name, argTypes, args);
  }
   //cout << "END OF RPCCALL, yeah !!" << endl;
   return 0;
}







server_db SDB;

int rpcRegister(char* name, int* argTypes, skeleton f)
{
    // register in the local data base
    SDB.insert(name, f);

    //SDB.print();

    char SERVER_ADDRESS[100];
    int PORT;
    int MESSAGE_LENGTH;
    int MESSAGE_TYPE = REGISTER;

    int socket_fd_for_server = socket_fd;
    int server_to_binder_socket = socket_fd_binder;


   char* binder_addr = getenv("BINDER_ADDRESS");
   char* binder_port = getenv("BINDER_PORT");

   gethostname(SERVER_ADDRESS, 99);

   socklen_t server_len = sizeof(server_addr);
   getsockname(socket_fd_for_server, (struct sockaddr *)&server_addr, &server_len);

   PORT = ntohs(server_addr.sin_port);


   // get size of each elements
   int size_of_SERVER_ADDRESS = strlen(SERVER_ADDRESS) + 1;
   int size_of_PORT = sizeof(PORT);
   int size_of_name = strlen(name) + 1;
   int size_of_argTypes = argTypes_length_check(argTypes) * sizeof(int);

   MESSAGE_LENGTH = size_of_SERVER_ADDRESS + size_of_PORT + size_of_name + size_of_argTypes + 3;

   char main_message[MESSAGE_LENGTH];

   int counter = 0;

   char space = ' ';
   int size_of_space = sizeof(space);

  // pack all data;
   memcpy(main_message + counter, SERVER_ADDRESS, size_of_SERVER_ADDRESS);
   counter += size_of_SERVER_ADDRESS;

   memcpy(main_message + counter, &space, size_of_space);
   counter += size_of_space;

   memcpy(main_message + counter, &PORT, size_of_PORT);
   counter += size_of_PORT;

   memcpy(main_message + counter, &space, size_of_space);
   counter += size_of_space;

   memcpy(main_message + counter, name, size_of_name);
   counter += size_of_name;

   memcpy(main_message + counter, &space, size_of_space);
   counter += size_of_space;

   //memcpy(main_message + counter, argTypes, size_of_argTypes);
    for (int i = 0; i < size_of_argTypes / sizeof(int); i++) {
        memcpy(main_message + counter, &argTypes[i], sizeof(int));
        counter += sizeof(int);

        //cout << "counter" << i << " " << counter << endl;
    }

   int send_checker;

   // send message length to binder
   //cout << "message length:" << MESSAGE_LENGTH << endl;
  
   uint32_t MESSAGE_LENGTH_byte = htonl(MESSAGE_LENGTH);
   send_checker = send(server_to_binder_socket, &MESSAGE_LENGTH_byte, sizeof(uint32_t), 0);
   if (send_checker < 0)
   {
       cerr << "can't find the binder, first send error" << endl;
       return -10;
   }

   //cout << "message type: " << MESSAGE_TYPE << endl;

   uint32_t MESSAGE_TYPE_byte = htonl(MESSAGE_TYPE);
   send_checker =  send(server_to_binder_socket, &MESSAGE_TYPE_byte, sizeof(uint32_t), 0);
   if (send_checker < 0)
   {
       cerr << "can't find the binder, second send error" << endl;
       return -11;
   }

   send_checker = sendall(server_to_binder_socket, main_message, &MESSAGE_LENGTH);
   if (send_checker < 0)
   {
       cerr << "can't find the binder, third send error" << endl;
       return -12;
   }

   unsigned char return_code[4];
   int recv_checker = recv(server_to_binder_socket, return_code, sizeof return_code, 0);

   int return_code_int = (return_code[0] << 24) | (return_code[1] << 16) | (return_code[2] << 8) | return_code[3];
   
   //cout << "return_code is: " << return_code_int << endl;




   return 0;
}


int rpcExecute()
{
   //cout << endl;
   //cout << "get into rpcExecute !!" << endl;
   //SDB.print();

   //cout << "socket fd for execute: " << socket_fd << endl;

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number
    int socket_fd_accept;
    int send_recv_checker;

    struct sockaddr_in client_addr;

    socklen_t client_len;


    unsigned char message_length[4];
    int message_length_int;

    unsigned char message_type[4];
    int message_type_int;




    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

     // add the listener to the master set
    FD_SET(socket_fd, &master);

    // keep track of the biggest file descriptor
    fdmax = socket_fd; // so far, it's this one

           // run through the existing connections looking for data to read
    for(;;) {

        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

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
                         //cout << "client accepted, add accept socket to master" << endl;
                    }
                } else {
                    // handle data from a client
                    if ((send_recv_checker = recv(i, message_length, sizeof message_length, 0)) <= 0) {
                        // got error or connection closed by client
                        if (send_recv_checker == 0) {
                            // connection closed
                            //printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client                     
                        // whole message length
                        message_length_int = (message_length[0] << 24) | (message_length[1] << 16) | (message_length[2] << 8) | message_length[3];
                        
                        //cout << endl;
                        //cout << "rpcExecute: message_length_int: " << message_length_int << endl;

                        send_recv_checker = recv(i, message_type, sizeof message_type, 0);
                        message_type_int = (message_type[0] << 24) | (message_type[1] << 16) | (message_type[2] << 8) | message_type[3];
                        
                        //cout << "rpcExecute: message_type_int: " << message_type_int << endl;

                        if (message_type_int == TERMINATE)
                        {
                            //cout << "TERMINATE server" << endl;
                            return 0;
                        }

                        if (message_type_int != EXECUTE)
                        {
                            cerr << "NOT EXECUTE, SOMETHING MUST BE wrong" << endl;
                            return -101;
                        }

                        // create a buffer for message
                        char recv_buffer[message_length_int];

                        send_recv_checker = recv(i, recv_buffer, sizeof(recv_buffer), 0);

                        // unpacket info;
                        // processing data
                       int sizes[2];
                       int size_index = 0;
                   for (int j = 0; j < message_length_int; j++) {
                   if (recv_buffer[j] == ' ') {
                       //cout << "space pos: " << i << endl;
                          sizes[size_index] = j;
                          size_index++;
                    }
                  }

                  int function_name_length = sizes[0];
                  char function_name[function_name_length];
                  memcpy(function_name, recv_buffer, function_name_length);

                  //cout << "function name: " << function_name << endl;

   
                   // argtypes
                  int argTypes_length = ( (sizes[1] - sizes[0] - 1) / sizeof(int) );
                  //cout << "argTypes_length: " << argTypes_length << endl; 
                  //cout << endl;

                  int argTypes[argTypes_length];

                  for (int j=0; j<argTypes_length; j++)
                  {
                     memcpy(&argTypes[j], recv_buffer + sizes[0] + 1 + j*sizeof(int), sizeof(int));

                     //cout << "argtypes" << j << ": " << argTypes[j] << endl;
                  }
                  //cout << endl;


                 //void* args[argTypes_length - 1];
                  void** args = (void **)malloc((argTypes_length-1) * sizeof(void *));

                 int total_size = 0;

                 for (int j=0; j<argTypes_length - 1; j++)
                 {
                    int checker = 0;

                    int array_size = get_array_size(argTypes[j]);
                    int arg_type_size = args_size_helper(argTypes[j]);

                    //cout << "array_size: " << array_size << endl;
                    //cout << "arg_type_size: " << arg_type_size << endl;
                    //cout << "here 1"<< endl;


                     int copy_size = arg_type_size * array_size;
                     

                     args[j] = (void *)malloc(copy_size);

                    memcpy(args[j], recv_buffer + sizes[1] + 1 + total_size, copy_size);

                    total_size += copy_size;

                    //cout << "here 2"<< endl;

                    //cout << "arg" << j << ": " << *((int *)(args[j])) << endl;

                    //cout << "here 3" << endl;
                    //cout << endl;
                  }

                  
                  skeleton s_func = SDB.get_skeleton(function_name);
                  if (s_func == NULL)
                  {
                     return NO_PROCEDURE;
                  }

                  int func_return_val = s_func(argTypes, args);
                  //cout << "skeleton return val: " << func_return_val << endl;

                  //cout << "return val in arg0: " << *((int *)(args[0])) << endl;


                  // calculate complete, send back to client
                  
                  if (func_return_val == 0)
                  {
                     int send_recv_checker = buffer_send(i, EXECUTE_SUCCESS, function_name, argTypes, args);
                     if (send_recv_checker != 0)
                     {
                        return send_recv_checker;
                     }
                  }

                  if (func_return_val != 0)
                  {
                     // send [execute_failure , reasoncode]
                     int MESSAGE_LENGTH = 4;
                     int message_type = EXECUTE_FAILURE;
                     int reasoncode = EXECUTE_SERVER_FAILURE;

                      uint32_t MESSAGE_LENGTH_byte = htonl(MESSAGE_LENGTH);
                      send_recv_checker = send(i, &MESSAGE_LENGTH_byte, sizeof(uint32_t), 0);

                      uint32_t MESSAGE_TYPE_byte = htonl(message_type);
                      send_recv_checker = send(i, &MESSAGE_TYPE_byte, sizeof(uint32_t), 0);

                      uint32_t reasoncode_byte = htonl(reasoncode);
                      send_recv_checker = send(i, &reasoncode_byte, sizeof(uint32_t), 0);




                  }
              

                        



                  
                    }
                }
            } 
        }
    } 
}


int rpcTerminate()
{
    //use socket_terminate to connect binder
    // send [execute_failure , reasoncode]

    struct hostent* host_server;
    struct addrinfo *server_addr, *try_temp;
    int socket_from_client_to_binder;
    struct addrinfo hints;
    int status_checker;


    char* SA = getenv("BINDER_ADDRESS");
    char* PO = getenv("BINDER_PORT");

    if (SA==NULL || PO==NULL)
    {
       cerr << "client can't locate the binder" << endl;
       return -37;
    }


   host_server = gethostbyname(SA);
   if (host_server == NULL)
   {
       cerr << "can't get the host_server on client" << endl;
       exit(1);
   }

   int port_number = atoi(PO);


   memset(&hints, 0, sizeof hints); // make sure the struct is empty
   hints.ai_family = AF_INET;     // IPv4
   hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
   hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

   if ((status_checker = getaddrinfo(host_server->h_name, PO, &hints, &server_addr)) != 0) {
      cerr << "getaddrinfo error on client" << endl;
      exit(1);
   }
 
   for(try_temp = server_addr; try_temp != NULL; try_temp = try_temp->ai_next) {
        if ((socket_from_client_to_binder = socket(try_temp->ai_family, try_temp->ai_socktype,
                try_temp->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(socket_from_client_to_binder, try_temp->ai_addr, try_temp->ai_addrlen) == -1) {
            close(socket_from_client_to_binder);
            perror("client: connect");
            continue;
        }

        break;
    }


    int MESSAGE_LENGTH = 4;
    int message_type = TERMINATE;
    int reasoncode = TERMINATE;

    int send_recv_checker;
    //cout << "call termiate: " << TERMINATE << endl;
   
    uint32_t MESSAGE_LENGTH_byte = htonl(MESSAGE_LENGTH);
    send_recv_checker = send(socket_from_client_to_binder, &MESSAGE_LENGTH_byte, sizeof(uint32_t), 0);
    if (send_recv_checker < 0)
    {
       cerr << "error on first termiate message." << endl;
       return send_recv_checker;
    }

    uint32_t MESSAGE_TYPE_byte = htonl(message_type);
    send_recv_checker = send(socket_from_client_to_binder, &MESSAGE_TYPE_byte, sizeof(uint32_t), 0);
     if (send_recv_checker < 0)
    {
       cerr << "error on second termiate message." << endl;
       return send_recv_checker;
    }

    uint32_t reasoncode_byte = htonl(reasoncode);
    send_recv_checker = send(socket_from_client_to_binder, &reasoncode_byte, sizeof(uint32_t), 0);
     if (send_recv_checker < 0)
    {
       cerr << "error on third termiate message." << endl;
       return send_recv_checker;
    }

    //cout << "TERMINATed ..." << endl;

    return 0;
}