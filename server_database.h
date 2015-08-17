#ifndef __server_database__
#define __server_database__

#include "rpc.h"
#include <iostream>
#include <map>

using namespace std;


class server_db {

private:
	map<string, skeleton> mymap;   // local database for server

public:
  int insert(char* name, skeleton f);

  skeleton get_skeleton(char* name);

  void print();


};





#endif