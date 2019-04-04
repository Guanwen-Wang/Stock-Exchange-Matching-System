#include <stdio.h>
#include <stdlib.h>

#include "server.h"

// main function
int main() {
  // become a daemon
  /*  pid_t mypid = getpid();
  pid_t pid, sid;
  int out;

  // deamon
  if (mypid != 1) {

    pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    }

    if (pid > 0) {
      exit(EXIT_SUCCESS);
    }

    umask(0);

    char buf[512]; // to store the cwd
    getcwd(buf, 512);
    chdir("/var/log");
    mkdir("./erss", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    chdir(buf);

    out = open("/var/log/erss/proxy.log", O_WRONLY | O_TRUNC | O_CREAT,
               S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
    dup2(out, 1);
    close(STDIN_FILENO);
    close(STDERR_FILENO);

    // close(STDOUT_FILENO);

    sid = setsid();
    if (sid < 0) {
      exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
      exit(EXIT_FAILURE);
    }
  }
  // docker
  else {
    // mkdir("./erss", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    out = open("/var/log/erss/proxy.log", O_WRONLY | O_TRUNC | O_CREAT,
               S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
    if (out == -1) {
      cout << "cannot open file" << endl;
      exit(EXIT_FAILURE);
    }
    dup2(out, 1);

    close(STDIN_FILENO);
    close(STDERR_FILENO);
    }*/

  // start
  Database db;
  db.init();
  db.drop_tables();
  db.create_tables();

  Server server;
  server.Run(db);
  // close(out);
  return 0;
}
