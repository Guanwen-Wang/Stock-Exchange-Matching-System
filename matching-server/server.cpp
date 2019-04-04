#include "server.h"

// new thread: deal with one request and send back the corresponding response
void new_request(int client_fd, Database db) {

  // receive the request from client
  vector<char> buff = receive(client_fd);
  // load xml parser
  pugi::xml_document doc;
  pugi::xml_parse_result res = doc.load_string(buff.data());

  string response = "";
  if (!res || buff.empty()) {
    // error when parsing xml
    cout << "error: parsing xml fail" << endl;
    response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error>Illegal "
               "XML Format</error>\n";
    send_back(client_fd, response);
    close(client_fd);
    return;
  }

  if (doc.child("create")) {
    // create
    process_create(doc, response, db);
  } else if (doc.child("transactions")) {
    // transaction
    process_transaction(doc, response, db);
  } else {
    // error
    cout << "Illegal Request Tag" << endl;
    response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error>Illegal "
               "XML Tag</error>\n";
  }

  send_back(client_fd, response);
  close(client_fd);
  return;
}

//*************************Server class functions*****************************//

// Server Object Constructor, set up the server functionalities for proxy
// If any setup fail, exit the whole program
Server::Server() {
  memset(&host, 0, sizeof(host));
  host.ai_family = AF_UNSPEC;
  host.ai_socktype = SOCK_STREAM;
  host.ai_flags = AI_PASSIVE;
  status = getaddrinfo(NULL, SERVERPORT, &host, &host_list);

  if (status != 0) {
    cerr << "Error: address issue" << endl;
    exit(EXIT_FAILURE);
  }

  server_sockfd = socket(host_list->ai_family, host_list->ai_socktype,
                         host_list->ai_protocol);

  if (server_sockfd == -1) {
    cerr << "Error: socket creation failed" << endl;
    exit(EXIT_FAILURE);
  }

  int yes = 1;
  status =
      setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  if (status == -1) {
    cerr << "Error: socket operation fail" << endl;
    exit(EXIT_FAILURE);
  }

  status = bind(server_sockfd, host_list->ai_addr, host_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: Binding fail" << endl;
    exit(EXIT_FAILURE);
  }

  status = listen(server_sockfd, 10240);
  if (status == -1) {
    cerr << "Error: listen fail" << endl;
    exit(EXIT_FAILURE);
  }
}

/*
  Run the engine, accept connection from clients and start new thread
  to process requests
 */
void Server::Run(Database db) {

// int id = 0;
#if THREAD_POOL
  // ctpl::thread_pool pool(5);
  threadpool executor{50};
#endif
  while (1) {
    // id++;
    sin_size = sizeof(their_addr);
    int client_fd =
        accept(server_sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (client_fd == -1) {
      cout << " Error: accepting connection fail" << endl;
      continue;
    }
    // need to pass in client_id, client ip addr, client port num?
    // Gary: pass one more arg in new_request: db
#if THREAD_POOL
    /*
    pool.push(new_request, client_fd,
              string(inet_ntoa(((struct sockaddr_in *)&their_addr)->sin_addr)),
              db);*/
    executor.commit(new_request, client_fd, db).get();
#else
    thread t(new_request, client_fd, db);
    t.join();
#endif
  }
  close(server_sockfd);
}
