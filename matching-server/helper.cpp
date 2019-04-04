#include "helper.h"
#define BUFF_SIZE 409600
std::mutex mtx;

vector<char> receive(int &client_fd) {
  vector<char> buff(BUFF_SIZE);
  int data_len = recv(client_fd, &(buff.data()[0]), BUFF_SIZE, 0);
  int index = data_len;
  int total_len = 0;
  try {
    total_len =
        stoi(string(buff.data()).substr(0, string(buff.data()).find('\n') + 1));
  } catch (const exception &e) {
    return {};
  }

  cout << "=========================" << endl;
  cout << "starting receiving" << endl;
  cout << "data_len: " << data_len << endl;
  cout << "total_len: " << total_len << endl;
  if (data_len >= BUFF_SIZE) {
    while (data_len != 0) {
      buff.resize(index + 1024);
      data_len = recv(client_fd, &(buff.data()[index]), 1024, 0);
      index += data_len;
      if (data_len < 1024 && data_len > 0) {
        buff.resize(index);
      }
      if (data_len <= 0 || total_len <= index) {
        break;
      }
      if (string(buff.begin(), buff.end()).find("</create>") != string::npos) {
        break;
      }
      cout << buff.data() << endl;
    }
  }

  cout << "done receiving" << endl;
  // get rid of byte len at the beginning
  while (isdigit(*buff.begin())) {
    buff.erase(buff.begin());
  }
  buff.erase(buff.begin());
  return buff;
}

// send back results
void send_back(int &client_fd, string &response) {
  cout << "start sending back" << endl;
  size_t sent = 0;
  vector<char> res(response.begin(), response.end());
  while (1) {
    if (sent + 1024 < res.size()) {
      sent += send(client_fd, &(res.data()[sent]), 1024, 0);
    } else {
      sent += send(client_fd, &(res.data()[sent]), res.size() - sent, 0);
      break;
    }
  }
  cout << "done sending back" << endl;
  return;
}

// traverse xml file, create account and symbol for each account
int process_create(pugi::xml_document &doc, string &response, Database &db) {
  std::lock_guard<std::mutex> lck (mtx);

  pugi::xml_document res;
  pugi::xml_node head = res.append_child("result");

  for (pugi::xml_node e : doc.child("create")) {
    if (string(e.name()) == "account") {
      // acc
      string acc_id = e.first_attribute().value();
      int balance = stoi(e.last_attribute().value());

      // interact with database to create new account
      if (db.create_account(acc_id, balance)) {
        // generate success response: <created ...>
        head.append_child("created").append_attribute("id").set_value(
            acc_id.c_str());
      } else {
        // generate error response: <error ...>account already exists
        pugi::xml_node tp = head.append_child("error");
        tp.append_attribute("id").set_value(acc_id.c_str());
        tp.text().set("Account cannot be created");
      }

    } else if (string(e.name()) == "symbol") {
      // sym
      string sym_name = e.first_attribute().value();

      // add number of shares of symbol into account
      for (pugi::xml_node a : e) {
        string acc_id = a.first_attribute().value();
        int num_share = stoi(a.text().data().value());

        // interact with database to create a new position
        if (db.create_position(sym_name, acc_id, num_share)) {
          // generate success response: <created ...>
          pugi::xml_node tp = head.append_child("created");
          tp.append_attribute("sym").set_value(sym_name.c_str());
          tp.append_attribute("id").set_value(acc_id.c_str());
        } else {
          // generate error response: <error ...>
          pugi::xml_node tp = head.append_child("error");
          tp.append_attribute("sym").set_value(sym_name.c_str());
          tp.append_attribute("id").set_value(acc_id.c_str());
          tp.text().set("Symbol creation failed");
        }
      }

    } else {
      cout << "Illegal create Tag" << endl;
      return -1;
    }
  }
  stringstream buffer;
  res.save(buffer);
  response = buffer.str();
  return 0;
}

// traverse xml file, order/query/cancel any order specified
int process_transaction(pugi::xml_document &doc, string &response,
                        Database &db) {
  std::lock_guard<std::mutex> lck (mtx);

  pugi::xml_document res;
  pugi::xml_node head = res.append_child("result");

  string acc_id = doc.child("transactions").first_attribute().value();
  // check acc_id. if not valid, then generate <error ...> and return
  if (!db.is_acc_valid(acc_id)) {
    // generate <error>invalid account number</error>
    head.append_child("error").text().set("Invalid account ID");
    stringstream buffer;
    res.save(buffer);
    response = buffer.str();
    return -1;
  }

  for (pugi::xml_node t : doc.child("transactions")) {
    // opt
    string opt = t.name();
    if (opt == "order") {
      // order
      string sym = "";
      int amount = 0;
      double limit = 0;
      int track = 0;
      for (pugi::xml_attribute attr = t.first_attribute(); attr;
           attr = attr.next_attribute()) {
        if (track == 0) {
          sym = attr.value();
          track++;
        } else if (track == 1) {
	  //          cout << "true amount: " << attr.value() << endl;
          amount = stoi(string(attr.value()));
          track++;
        } else if (track == 2) {
	  //          cout << "true limit: " << attr.value() << endl;
          limit = stod(string(attr.value()));
          track++;
        }
      }

      //      cout << "parsed amount: " << amount << endl;
      //      cout << "parsed limit: " << limit << endl;

      pugi::xml_node tp = head.append_child("error");
      tp.append_attribute("sym").set_value(sym.c_str());
      tp.append_attribute("amount").set_value(to_string(amount).c_str());
      tp.append_attribute("limit").set_value(to_string(limit).c_str());

      // first check the balance or amount
      if (amount > 0) { // buy order
                        // check if balance sufficient
        if (!db.check_balance(acc_id, amount, limit)) {
          // generate <error>insufficient funds</error> and continue
          tp.text().set("Insufficient funds");
          continue;
        }
      } else if (amount < 0) { // sell
                               // check if amount sufficient
        if (!db.check_amount(acc_id, amount, sym)) {
          // generate <error>insufficient amount</error> and continue
          tp.text().set("Insufficient amount");
          continue;
        }
      } else if (amount == 0) {
        tp.text().set("Amount cannot be 0");
        continue;
      }

      // interact with data base to create a new order
      int trans_id = db.create_order(acc_id, sym, amount, limit);

      head.remove_child(tp);
      // tp.set_name();

      // find if there is matching order
      vector<int> exec_price;
      vector<int> exec_amount;
      if (db.execute_order(trans_id, acc_id, sym, amount, limit, exec_price,
                           exec_amount)) {
        // order already been executed
        int sum = 0;
        for (auto i : exec_amount) {
          sum += i;
        }
        // all executed
        if (abs(sum) == abs(amount)) {
          for (size_t i = 0; i < exec_amount.size(); i++) {
            pugi::xml_node executed = head.append_child("executed");
            executed.append_attribute("sym").set_value(sym.c_str());
            executed.append_attribute("amount").set_value(
                to_string(exec_amount[i]).c_str());
            executed.append_attribute("limit").set_value(
                to_string(exec_price[i]).c_str());
          }
        }
        // partially executed
        else {
          pugi::xml_node opened = head.append_child("opened");
          opened.append_attribute("sym").set_value(sym.c_str());
          if (amount < 0) {
            opened.append_attribute("amount").set_value(
                to_string(amount + sum).c_str());
          } else if (amount > 0) {
            opened.append_attribute("amount").set_value(
                to_string(amount - sum).c_str());
          }
          opened.append_attribute("limit").set_value(to_string(limit).c_str());

          for (size_t i = 0; i < exec_amount.size(); i++) {
            pugi::xml_node executed = head.append_child("executed");
            executed.append_attribute("sym").set_value(sym.c_str());
            executed.append_attribute("amount").set_value(
                to_string(exec_amount[i]).c_str());
            executed.append_attribute("limit").set_value(
                to_string(exec_price[i]).c_str());
          }
        }

      } else {
        // no matching order and return <open>
        tp.set_name("opened");
      }

    }

    // operation query
    else if (opt == "query") {
      string trans_id = t.first_attribute().value();

      int open_share = -1;
      vector<canceled> query_canceled;
      vector<executed> query_executed;

      pugi::xml_node status = head.append_child("status");
      status.append_attribute("id").set_value(to_string(trans_id).c_str());

      if (db.query(acc_id, trans_id, open_share, query_canceled,
                   query_executed)) {
        // query success and generate <open>, <canceled>, <executed>

        // open share exist
        if (open_share != -1) {
          status.append_child("opened").append_attribute("shares").set_value(
              to_string(open_share).c_str());
        }
        // canceled order exist
        if (!query_canceled.empty()) {
          for (auto e : query_canceled) {
            pugi::xml_node cancel = status.append_child("canceled");
            cancel.append_attribute("shares").set_value(
                to_string(e.share).c_str());
            cancel.append_attribute("time").set_value(
                to_string(e.time).c_str());
          }
        }
        // executed order exist
        if (!query_executed.empty()) {
          for (auto e : query_executed) {
            pugi::xml_node exec = status.append_child("executed");
            exec.append_attribute("shares").set_value(
                to_string(e.share).c_str());
            exec.append_attribute("price").set_value(
                to_string(e.price).c_str());
            exec.append_attribute("time").set_value(to_string(e.time).c_str());
          }
        }

      } else {
        // query error, no such trans_id
        status.append_child("error").text().set("Transaction ID not found");
      }
    }

    // operation cancel
    else if (opt == "cancel") {
      string trans_id = t.first_attribute().value();

      vector<canceled> cancel_canceled;
      vector<executed> cancel_executed;

      pugi::xml_node canceled = head.append_child("canceled");
      canceled.append_attribute("id").set_value(to_string(trans_id).c_str());

      if (db.cancel(acc_id, trans_id, cancel_canceled, cancel_executed)) {
        // cancel success and generate <cancel>, <executed>
        for (auto e : cancel_canceled) {
          pugi::xml_node can = canceled.append_child("canceled");
          can.append_attribute("shares").set_value(to_string(e.share).c_str());
          can.append_attribute("time").set_value(to_string(e.time).c_str());
        }
        for (auto e : cancel_executed) {
          pugi::xml_node exe = canceled.append_child("executed");
          exe.append_attribute("share").set_value(to_string(e.share).c_str());
          exe.append_attribute("price").set_value(to_string(e.price).c_str());
          exe.append_attribute("time").set_value(to_string(e.time).c_str());
        }
      } else {
        // cancel error
        canceled.append_child("error").text().set("Transaction ID not found");
      }

    }

    // invalid operation
    else {
      cout << "Illegal Tag in Transaction" << endl;
      return -1;
    }
  }

  stringstream buffer;
  res.save(buffer);
  response = buffer.str();
  return 0;
}
