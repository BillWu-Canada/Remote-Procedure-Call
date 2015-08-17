#ifndef __helper_code__
#define __helper_code__

#include <iostream>
#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>

using namespace std;

#define SUCCESS  0
#define FAIL  -1

#define SOCKET_CREATE_FAIL -111
#define CONNECT_FAIL -112


#define REGISTER_SUCCESS 200
#define REGISTER_WARNING 201
#define REGISTER_FAILURE -202

#define LOC_NO_SERVER -301
#define LOC_SERVER_FAILURE -302
#define LOC_BINDER_FAILURE -303

#define EXECUTE_NO_FUNCTION -401
#define EXECUTE_SERVER_FAILURE -402
#define NO_PROCEDURE -403

#define CACHE_NO_SERVER -501
#define CACHE_SERVER_FAILURE -502
#define CACHE_BINDER_FAILURE -503
#define CACHE_CACHED_SERVER_FAILURE -504

// rpcregister starting at 100
#define SERVER_REGISTER 100
#define REGISTER 101
#define EXECUTE 102
#define EXECUTE_SUCCESS 103
#define EXECUTE_FAILURE 104
#define TERMINATE 105
#define LOC_REQUEST 106
#define LOC_SUCCESS 107
#define LOC_FAILURE 108
#define CACHE_REQUEST 109
#define CACHE_SUCCESS 110
#define CACHE_FAILURE 111



int sendall(int s, char *buf, int *len)
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



int server_address_length_check(char* input)
{
	int length = 0;
	for(int i=0; true; i++)
	{
		if(input[i] == '\0')
		{
			return length;
		}
		length++;
	}
}

int argTypes_length_check(int* argTypes)
{
	int length = 0;
	if (argTypes == NULL)
	{
		return length;
	}

	for (int i=0; true; i++)
	{
		length++;

		if (argTypes[i] == 0)
		{
			return length;
		}
	}
}


int args_size_helper(int argType)
{
	int arg_property = (argType & 0x00FF0000) >> 16;

	  if (arg_property == ARG_CHAR)
    {
        return sizeof(char);        
    }
    else if (arg_property == ARG_SHORT)
    {
        return sizeof(short);                         
    }
    else if (arg_property == ARG_INT)
    {
        return sizeof(int);                         
    }
    else if (arg_property == ARG_LONG)
    {
        return sizeof(long);                       
    }
    else if (arg_property == ARG_DOUBLE)
    {
        return sizeof(double);                        
    }
    else if (arg_property == ARG_FLOAT)
    {
        return sizeof(float);                         
    }
    else 
    {
         return 0;
    }
}


int args_size_checker(int* argTypes)
{
	int args_total_size = 0;
	int argTypes_length = argTypes_length_check(argTypes);

	for(int i=0; i<argTypes_length-1; i++)
	{
		int array_size = argTypes[i] & 0x0000FFFF;

		if (array_size == 0)
		{
			array_size = 1;
		}

		int element_size = args_size_helper(argTypes[i]);

		if (element_size == 0)
		{
			return 0;
		} 

		args_total_size += array_size * element_size;
	}

	return args_total_size;
}

int get_array_size(int argType)
{
	int array_size = argType & 0x0000FFFF;
	
	if (array_size == 0)
    {
	    array_size = 1;
    }

    return array_size;
}


int buffer_send(int socket_fd, int message_type, char* name, int* argTypes, void** args)
{

     //cout << "get into buffer_send()" << endl;

     int size_of_name = strlen(name) + 1;
     int size_of_argTypes = argTypes_length_check(argTypes) * sizeof(int);
     int num_of_space = 1;

     int size_of_args = 0;

     if (args != NULL)
     {
     	// we have args
     	size_of_args = args_size_checker(argTypes);
     	num_of_space = 2;
     }

     int MESSAGE_LENGTH = size_of_name + size_of_argTypes + size_of_args + num_of_space;

     char send_buffer[MESSAGE_LENGTH];

     int counter = 0;

     char space = ' ';
     int size_of_space = sizeof(space);

     // packet message
     // packet name
    memcpy(send_buffer+counter, name, size_of_name);
    counter += size_of_name;

    memcpy(send_buffer+counter, &space, size_of_space);
    counter += size_of_space;
   
    // packet argtypes
    for (int i = 0; i < size_of_argTypes / sizeof(int); i++) {
        memcpy(send_buffer + counter, &argTypes[i], sizeof(int));
        counter += sizeof(int);

        // cout << "counter" << i << " " << counter << endl;
    }

    if (args != NULL)
    {
    	memcpy(send_buffer+counter, &space, size_of_space);
        counter += size_of_space;

       // packet args
       for (int i=0; i < argTypes_length_check(argTypes) - 1; i++)
       {
         //cout << i << endl;
         int array_size = argTypes[i] & 0x0000FFFF;
         if (array_size == 0)
         {
            array_size = 1;
         }

         int element_size = args_size_helper(argTypes[i]);

         int copy_size = array_size * element_size;

         memcpy(send_buffer+counter, args[i], copy_size);
         counter += copy_size;
       }

    }


    int send_checker;

    // send message to socketfd
   //cout << "send message length:" << MESSAGE_LENGTH << endl;
  
   uint32_t MESSAGE_LENGTH_byte = htonl(MESSAGE_LENGTH);
   send_checker = send(socket_fd, &MESSAGE_LENGTH_byte, sizeof(uint32_t), 0);
   
   if (send_checker < 0)
   {
       cerr << "first send error" << endl;
       return -60;
   }   

   int MESSAGE_TYPE = message_type;
   //cout << "send message type: " << MESSAGE_TYPE << endl;

   uint32_t MESSAGE_TYPE_byte = htonl(MESSAGE_TYPE);
   send_checker =  send(socket_fd, &MESSAGE_TYPE_byte, sizeof(uint32_t), 0);
   
   if (send_checker < 0)
   {
       cerr << "second send error" << endl;
       return -61;
   }


   send_checker = sendall(socket_fd, send_buffer, &MESSAGE_LENGTH);
   
   if (send_checker < 0)
   {
       cerr << "third send error" << endl;
       return -62;
   }




   return 0;
 

}


int final_unpacket(char* recv_buffer, int total_size, int* argTypes, void** args)
{

     //cout << "get into final_unpacket()" << endl;

      int sizes[2];
      int size_index = 0;

      for (int i = 0; i < total_size; i++) {
         if (recv_buffer[i] == ' ') {
                sizes[size_index] = i;
                size_index++;
         }
      }

      //cout << "sizes[0]: " << sizes[0] << endl;
      //cout << "sizes[1]: " << sizes[1] << endl;

      int argTypes_length = ( (sizes[1] - sizes[0] - 1) / sizeof(int) );
      //cout << "argTypes_length: " << argTypes_length << endl; 
      //cout << endl;

      for (int i=0; i<argTypes_length; i++)
      {
         memcpy(&argTypes[i], recv_buffer + sizes[0] + 1 + i*sizeof(int), sizeof(int));

         //cout << "received argtypes" << i << ": " << argTypes[i] << endl;
       }
         //cout << endl;

       int total_size_1 = 0;

       for (int i=0; i<argTypes_length - 1; i++)
       {
         int checker = 0;

         int array_size = get_array_size(argTypes[i]);
         int arg_type_size = args_size_helper(argTypes[i]);


          int copy_size = arg_type_size * array_size;
          

          //args[i] = (void *)malloc(copy_size);

          memcpy(args[i], recv_buffer + sizes[1] + 1 + total_size_1, copy_size);

          total_size_1 += copy_size;

          //cout << "arg" << i << ": " << *((int *)(args[i])) << endl;
        }


        return 0;
      


}




#endif