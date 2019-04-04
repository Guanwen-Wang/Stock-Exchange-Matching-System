#ifndef SERVER_H
#define SERVER_H

// external lib
#include "database.h"
#include "helper.h"
//#include "thread_pool/cptl.h"
#include "thread_pool/threadpool.h"

// C++ Lib
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
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace std;

#define SERVERPORT "12345"
#define THREAD_POOL 1

class Server {
private:
  /* Engine server variables  */
  int server_sockfd;
  int status;
  struct addrinfo host;
  struct addrinfo *host_list;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  /**********************************/

public:
  Server();
  void Run(Database db);
  ~Server() {}
};

#endif
