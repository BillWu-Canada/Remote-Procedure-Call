#include "server_database.h"



int server_db::insert(char* name, skeleton f)
{
	string key(name);
	map<string, skeleton>::iterator it;

	it = mymap.find(key);

	if (it == mymap.end())
	{
		// not found the value, insert it
		mymap.insert( pair<string, skeleton>(key, f) );
		return 0;   //insert successfully
	}

	return 1;    // already contain the same info
}

skeleton server_db::get_skeleton(char* name)
{
     string key(name);
	 map<string, skeleton>::iterator it;

	 it = mymap.find(key);

	 if (it == mymap.end())
	{
		// not found the value, return error code
		cout << "not find skeleton which should be found !!!" << endl;
		return NULL;
	}

	return it->second;

}



void server_db::print()
{
    map<string, skeleton>::iterator it;

    for(it=mymap.begin(); it!=mymap.end(); it++)
    {
    	cout << it->first << " => " << it->second << endl;
    }

}