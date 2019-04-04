#ifndef HELPER_H
#define HELPER_H

#include "database.h"

#include "pugixml/pugixml.hpp"
#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <error.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

vector<char> receive(int &);
void send_back(int &, string &);
int process_create(pugi::xml_document &, string &, Database &);
int process_transaction(pugi::xml_document &, string &, Database &);

#endif
