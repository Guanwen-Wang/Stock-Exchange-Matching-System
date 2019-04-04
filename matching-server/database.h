#ifndef DATABASE_H
#define DATABASE_H

#include "mystruct.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <mutex>

#define DOCKER 1

using namespace std;
using namespace pqxx;

class Database {
private:
  connection *C;

public:
  Database() {}

  void init();
  void drop_tables();
  void create_tables();
  int create_account(string acc_id, int balance);
  int create_position(string sym_name, string acc_id, int num_share);
  int create_order(string acc_id, string sym, int amount, double limit);

  int query(string acc_id, string trans_id, int &open_share, vector<canceled> &query_canceled, vector<executed> &query_executed);
  int cancel(string acc_id, string trans_id, vector<canceled> &cancel_canceled,
             vector<executed> &cancel_executed);

  int check_balance(string acc_id, int amount, double limit);
  int check_amount(string acc_id, int amount, string sym);
  bool is_acc_valid(string acc_id);

  int execute_order(int trans_id, string acc_id, string sym, int amount, double limit, vector<int> &exec_price, vector<int> &exec_amount);
  
  ~Database() { C->disconnect(); }
};

#endif
