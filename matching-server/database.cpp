#include "database.h"

void execute_order_single(int trans_id, string acc_id, string sym, int amount, double limit, vector<int> &exec_price, vector<int> &exec_amount, int sell_id, int buy_id, int seller_id, int buyer_id, int curr_time, int after_exec_amount, connection * C);

void Database::init() {
  try {
#if DOCKER
    C = new connection("dbname=postgres user=postgres password=passw0rd host=stock_db port=5432");
#else
    C = new connection("dbname=postgres user=postgres password=passw0rd");    
#endif

    if (C->is_open()) {
      cout << "Opened database successfully: " << C->dbname() << endl;
    } else {
      cout << "Can't open database" << endl;
      return;
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    return;
  }
}

void Database::drop_tables() {
  string drop_table_sql = "DROP TABLE ACCOUNT,POSITION,ORDER_OPEN,EXECUTION,CANCEL;";

  work W(*C);
  W.exec( drop_table_sql );
  W.commit();
  cout << "Successfully drop all tables" << endl;
}

void Database::create_tables() {
  /****  Generate SQL string ****/
  string account_sql = "CREATE TABLE ACCOUNT("				\
    "ACCOUNT_ID     INT        PRIMARY KEY   NOT NULL,"			\
    "BALANCE        INT        NOT NULL);";
  
  string position_sql =				\
    "CREATE TABLE POSITION("			\
    "POSITION_ID    SERIAL      PRIMARY KEY,"				\
    "ACCOUNT_ID     INT         REFERENCES ACCOUNT(ACCOUNT_ID)  NOT NULL," \
    "SYM            VARCHAR     NOT NULL,"				\
    "AMOUNT         INT         NOT NULL);";
  
  string order_sql =\
    "CREATE TABLE ORDER_OPEN("			\
    "ORDER_ID       SERIAL      PRIMARY KEY,"				\
    "ACCOUNT_ID     INT         REFERENCES ACCOUNT(ACCOUNT_ID)  NOT NULL," \
    "SYM            VARCHAR     NOT NULL,"				\
    "AMOUNT         INT         NOT NULL,"				\
    "PRICE          REAL        NOT NULL,"				\
    "TIME           BIGINT      NOT NULL);";
  
  string execution_sql =			\
    "CREATE TABLE EXECUTION("			\
    "EXEC_ID        SERIAL      PRIMARY KEY,"	\
    "BUYER_ID       INT         NOT NULL,"	\
    "SELLER_ID      INT         NOT NULL,"	\
    "BUY_ID         INT         NOT NULL,"	\
    "SELL_ID        INT         NOT NULL,"	\
    "SYM            VARCHAR     NOT NULL,"	\
    "PRICE          REAL        NOT NULL,"	\
    "AMOUNT         INT         NOT NULL,"	\
    "TIME           BIGINT      NOT NULL);";

  string cancel_sql =				\
    "CREATE TABLE CANCEL("			\
    "CANCEL_ID      SERIAL     PRIMARY KEY,"	\
    "ACCOUNT_ID     INT        NOT NULL,"	\
    "TRANS_ID       INT        NOT NULL,"	\
    "AMOUNT         INT        NOT NULL,"	\
    "TIME           BIGINT     NOT NULL);";

  /***** Create a transactional object *****/
  work W(*C);

  /****** Execute SQL query ******/
  W.exec(account_sql);
  W.exec(position_sql);
  W.exec(order_sql);
  W.exec(execution_sql);
  W.exec(cancel_sql);
  W.commit();
  cout << "Successfully created all tables" << endl;
}

int Database::create_account(string acc_id, int balance) {
  // check if account already exist
  // if exist => return 0
  // create success => return 1
  // first check whether account already exist
  string sql;
  try {
    nontransaction N(*C);
    sql = "SELECT ACCOUNT_ID from ACCOUNT WHERE ACCOUNT_ID=" + acc_id + ";";
    result R(N.exec(sql));
    auto it = R.begin();
    if (it != R.end()) {
      return 0;
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
  }

  // account not exist, then create new account
  string create_account_sql = "INSERT INTO ACCOUNT (ACCOUNT_ID, BALANCE) VALUES (" + acc_id + ", " + to_string(balance) + ");";

  work W(*C);
  W.exec(create_account_sql);
  W.commit();
  return 1;
}

int Database::create_position(string sym_name, string acc_id, int num_share) {
  // if invalid acc_id, error => return 0
  // on success => return 1
  try {
    // first check if acc_id valid
    work W(*C);
    string is_id_valid_sql = "SELECT ACCOUNT_ID FROM ACCOUNT WHERE ACCOUNT_ID=" + acc_id + ";";
    result R1( W.exec(is_id_valid_sql) );

    auto it1 = R1.begin(); 
    if (it1 == R1.end())
      return 0; // acc_id invalid

    // then check if the position exists
    string is_pos_exist_sql = "SELECT AMOUNT from POSITION WHERE ACCOUNT_ID=" + acc_id + " AND SYM='" + sym_name + "';";
    result R2( W.exec(is_pos_exist_sql));

    auto it2 = R2.begin(); 
    if (it2 == R2.end()) { // position not exist, then create
      string create_pos_sql =						\
	"INSERT INTO POSITION (ACCOUNT_ID, SYM, AMOUNT) "		\
	"VALUES (" +							\
	acc_id + ", '" + sym_name + "', " + to_string(num_share) + ");";
      
      //work W(*C);
      W.exec(create_pos_sql);
      W.commit();
      return 1;
    } else { // position already exist, then update
      int new_amount = it2[0].as<int>() + num_share;
      string update_pos_sql =						\
	"UPDATE POSITION set AMOUNT=" + to_string(new_amount) +		\
	" WHERE ACCOUNT_ID=" + acc_id + " AND SYM='" + sym_name + "';";

      //work W(*C);
      W.exec(update_pos_sql);
      W.commit();
      return 1;
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    return 0;
  }
  return 1;
}

int Database::create_order(string acc_id, string sym, int amount,
                           double limit) {
  // on success, return trans_id
  // on error, return -1
  int trans_id = -1;
  try {
    time_t curr_time = time(NULL);
    string create_order_sql =\
        "INSERT INTO ORDER_OPEN (ACCOUNT_ID, SYM, AMOUNT, PRICE, TIME) VALUES (" +\
        acc_id + ", '" + sym + "', " + to_string(amount) + ", " +\
        to_string(limit) + ", " + to_string(curr_time) + ");";

    work W(*C);
    W.exec(create_order_sql);
    W.commit();

    // get the trans_id
    nontransaction N(*C);
    string get_trans_id_sql = "SELECT ORDER_ID FROM ORDER_OPEN "\
                              "WHERE TIME=" +\
                              to_string(curr_time) + ";";

    result R( N.exec(get_trans_id_sql));
    for (auto it = R.begin(); it != R.end(); ++it) {
      trans_id = it[0].as<int>();
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
  }
  return trans_id;
}

int Database::execute_order(int trans_id, string acc_id, string sym, int amount,
                            double limit, vector<int> &exec_price, vector<int> &exec_amount) {
  // find match order
  // insert item into EXECUTION table
  // erase item from ORDER_OPEN table
  // Note: partically executed: updata ORDER_OPEN table
  // update seller's balance
  // update buyer's position
  try {
    int sell_id = -1;
    int buy_id = -1;
    int seller_id = -1;
    int buyer_id = -1;
    int curr_time = time(NULL);
    int after_exec_amount = -1;
    work N(*C);
    
    if (amount > 0) { // buy order
      //******* find match order ******//
      string find_match_sql =\
          "SELECT ORDER_ID, ACCOUNT_ID, AMOUNT, PRICE from ORDER_OPEN WHERE "\
          "AMOUNT<0 AND PRICE<=" + to_string(limit) + " AND SYM='" + sym + "' AND ACCOUNT_ID!=" + acc_id  + " ORDER BY PRICE DESC, TIME ASC;";
      result R1( N.exec(find_match_sql) );
      for(auto it = R1.begin(); it != R1.end(); ++it){
	sell_id = it[0].as<int>();
	buy_id = trans_id;
	exec_price.push_back(limit);      
	exec_amount.push_back( min(amount, -it[2].as<int>()) );
	
	after_exec_amount = amount + it[2].as<int>();
	seller_id = it[1].as<int>();
	buyer_id = stoi(acc_id);
	N.commit();
	// execute match order once
	execute_order_single(trans_id, acc_id, sym, amount, limit, exec_price, exec_amount, sell_id, buy_id, seller_id, buyer_id, curr_time, after_exec_amount, C);
	
	if(after_exec_amount <= 0)
	  break;
	else
	  amount = after_exec_amount;	
      }
    } else if (amount < 0) { // sell order
      //******* find match order ******//
      string find_match_sql =						\
          "SELECT ORDER_ID, ACCOUNT_ID, AMOUNT, PRICE from ORDER_OPEN WHERE "\
          "AMOUNT>0 AND PRICE>=" + to_string(limit) + " AND SYM='" + sym + "' AND ACCOUNT_ID!=" + acc_id + " ORDER BY PRICE DESC, TIME ASC;";
      result R1( N.exec(find_match_sql));
      for(auto it = R1.begin(); it != R1.end(); ++it){
	buy_id = it[0].as<int>();
	sell_id = trans_id;
	exec_price.push_back( it[3].as<int>() );
	exec_amount.push_back( min(-amount, it[2].as<int>()) );
	
	after_exec_amount = amount + it[2].as<int>();
	buyer_id = it[1].as<int>();
	seller_id = stoi(acc_id);
	N.commit();
	// execute match order once
	execute_order_single(trans_id, acc_id, sym, amount, limit, exec_price, exec_amount, sell_id, buy_id, seller_id, buyer_id, curr_time, after_exec_amount, C);
	if(after_exec_amount >= 0)
	  break;
	else
	  amount = after_exec_amount;
      }
    }
  }
  catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    return 0;
  }
  return 1;
}

void execute_order_single(int trans_id, string acc_id, string sym, int amount, double limit, vector<int> &exec_price, vector<int> &exec_amount, int sell_id, int buy_id, int seller_id, int buyer_id, int curr_time, int after_exec_amount, connection * C){
  cout << "in execute_order_single ==>" << endl;
  try{
    //****** insert into EXECUTION table ******//
    string create_execution = "INSERT INTO EXECUTION (BUYER_ID, SELLER_ID, BUY_ID, SELL_ID, SYM, PRICE, AMOUNT, TIME) VALUES (" + to_string(buyer_id) + ", " + to_string(seller_id) + ", " + to_string(buy_id) + ", " + to_string(sell_id) + \
      ", '" + sym + "', " + to_string(exec_price.back()) +		\
      ", " + to_string(exec_amount.back()) + ", " +			\
      to_string(curr_time) + ");";
    work W(*C);
    W.exec(create_execution);
    W.commit();

    
    //****** update or erase from ORDER_OPEN table ******//
    if (after_exec_amount < 0) {
      // update sell order and delete buy order
      string update_sell_order =\
          "UPDATE ORDER_OPEN set AMOUNT=" + to_string(after_exec_amount) +\
          " WHERE ORDER_ID=" + to_string(sell_id) + ";";
      string delete_buy_order =\
          "DELETE from ORDER_OPEN WHERE ORDER_ID=" + to_string(buy_id) + ";";
      work W2(*C);
      W2.exec(update_sell_order);
      W2.exec(delete_buy_order);
      W2.commit();
    } else if (after_exec_amount > 0) {
      // delete sell order and update buy order
      string delete_sell_order =\
          "DELETE from ORDER_OPEN WHERE ORDER_ID=" + to_string(sell_id) + ";";
      string update_buy_order =\
          "UPDATE ORDER_OPEN set AMOUNT=" + to_string(after_exec_amount) +\
          " WHERE ORDER_ID=" + to_string(buy_id) + ";";
      work W2(*C);
      W2.exec(delete_sell_order);
      W2.exec(update_buy_order);
      W2.commit();
    } else {
      // erase both order
      string delete_sell_order =
          "DELETE from ORDER_OPEN WHERE ORDER_ID=" + to_string(sell_id) + ";";
      string delete_buy_order =
          "DELETE from ORDER_OPEN WHERE ORDER_ID=" + to_string(buy_id) + ";";
      work W2(*C);
      W2.exec(delete_sell_order);
      W2.exec(delete_buy_order);
      W2.commit();
    }
    
    
    //****** update seller's account -> balance ******//
    work W3(*C);
    string get_seller_balance =						\
        "SELECT BALANCE FROM ACCOUNT WHERE ACCOUNT_ID=" + to_string(seller_id) + ";";
    result R2( W3.exec(get_seller_balance));
    auto it2 = R2.begin();
    int seller_original_balance = it2[0].as<int>();
    int seller_updated_balance =				\
      seller_original_balance + exec_price.back() * exec_amount.back();
    string update_seller_acc =						\
      "UPDATE ACCOUNT set BALANCE=" + to_string(seller_updated_balance) + \
      " WHERE ACCOUNT_ID=" + to_string(seller_id) + ";";
    W3.exec(update_seller_acc);
    
    
    //****** update buyer's position -> amount ******//
    string get_buyer_position =						\
      "SELECT AMOUNT FROM POSITION WHERE ACCOUNT_ID=" + to_string(buyer_id) + \
      " AND SYM='" + sym + "';";
    cout << "buyer_id: " << buyer_id << endl;
    result R3( W3.exec(get_buyer_position));
    
    auto it3 = R3.begin();
    if(it3 != R3.end()){ // update existing position
      int buyer_original_amount = it3[0].as<int>();    
      int buyer_updated_amount = buyer_original_amount + exec_amount.back();
      cout << "buyer_original_amount: " << buyer_original_amount << endl;
      cout << "buyer_updated_amount: " << buyer_updated_amount << endl;
      string update_buyer_amt =						\
        "UPDATE POSITION set AMOUNT=" + to_string(buyer_updated_amount) + \
	" WHERE ACCOUNT_ID=" + to_string(buyer_id) + " AND SYM='" + sym + "';";
      
      W3.exec(update_buyer_amt);
      W3.commit();
    }
    else{ // create new position
      string create_new_pos =						\
	"INSERT INTO POSITION (ACCOUNT_ID, SYM, AMOUNT) VALUES (" + \
	to_string(buyer_id) + ", '" + sym + "', " + to_string(exec_amount.back()) + ");";
      
      W3.exec( create_new_pos );
      W3.commit();
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
  }
}

int Database::query(string acc_id, string trans_id, int &open_share,
                    vector<canceled> &query_canceled,
                    vector<executed> &query_executed) {
  // on success return 1;
  // on error return 0;
  try {
    //******** query open record ********//
    nontransaction N(*C);
    string query_open_sql =\
      "SELECT AMOUNT FROM ORDER_OPEN WHERE ORDER_ID=" + trans_id + " AND ACCOUNT_ID=" + acc_id + ";";

    result R1( N.exec(query_open_sql) );
    auto it = R1.begin();
    if(it != R1.end())
      open_share = it[0].as<int>();

    //******** query cancel record ********//
    string query_canceled_sql = "SELECT AMOUNT, TIME FROM CANCEL "	\
      "WHERE TRANS_ID=" + trans_id +				  \
      + " AND ACCOUNT_ID=" + acc_id + ";";

    result R2( N.exec(query_canceled_sql) );
    auto it2 = R2.begin();
    if(it2 != R2.end()){
      for (; it2 != R2.end(); ++it2) {
	canceled tmp = {it2[0].as<int>(), it2[1].as<long>()};
	query_canceled.push_back(tmp);
      }
    }
    
    //******** query executed record ********//
    string query_executed_sql = "SELECT AMOUNT, PRICE, TIME, BUYER_ID, SELLER_ID FROM EXECUTION WHERE BUY_ID=" + trans_id + " OR SELL_ID=" + trans_id + ";";

    result R3( N.exec(query_executed_sql) );
    auto it3 = R3.begin();
    if(it3 != R3.end()){
      for (; it3 != R3.end(); ++it3) {
	if(it3[3].as<int>() != stoi(acc_id) &&  it3[4].as<int>() != stoi(acc_id)){
	  continue;
	}
	executed tmp = {it3[0].as<int>(), it3[1].as<double>(), it3[2].as<long>()};
	query_executed.push_back(tmp);
      }
    }

    // if no record found, then return 0
    if(open_share == -1 && query_canceled.empty() && query_executed.empty())
      return 0;
    
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    return 0;
  }
  return 1;
}

int Database::cancel(string acc_id, string trans_id, vector<canceled> &cancel_canceled,
                     vector<executed> &cancel_executed) {
  // on success return 1
  // on error return 0
  try {
    //***** traverse ORDER_OPEN table and cancel the open record ******//
    // first get open order from ORDER_OPEN
    cout << "int cancel function" << endl;
    work W(*C);
    string open_order_sql =\
        "SELECT AMOUNT from ORDER_OPEN WHERE ORDER_ID=" + trans_id + ";";
    result R1( W.exec(open_order_sql));

    auto it1 = R1.begin();
    if(it1 != R1.end()){
      for (; it1 != R1.end(); ++it1) {
	// store info to vector
	time_t curr_time = time(NULL);
	canceled tmp = {it1[0].as<int>(), (long)curr_time};
	cancel_canceled.push_back(tmp);
	// insert canceled order into CANCEL
	string canceled_order_sql = "INSERT INTO CANCEL (ACCOUNT_ID, TRANS_ID, AMOUNT, TIME) VALUES (" + acc_id + ", " + trans_id + ", " + to_string(it1[0].as<int>()) +\
	  ", " + to_string(curr_time) + ");";
	W.exec(canceled_order_sql);
	//W.commit();
      }

      // then delete from ORDER_OPEN where id = trans_id
      string delete_sql = "DELETE from ORDER_OPEN WHERE ORDER_ID=" + trans_id + ";";
      //      work W(*C);
      W.exec(delete_sql);
    }
    W.commit();
      
    //***** traverse EXECUTION table and return executed record *****//
    cout << "cancel executed" << endl;
    nontransaction N(*C);
    string cancel_executed_sql = "SELECT AMOUNT, PRICE, TIME, BUYER_ID, SELLER_ID FROM EXECUTION WHERE BUY_ID=" + trans_id + " OR SELL_ID=" + trans_id + ";";

    result R2( N.exec(cancel_executed_sql));
    auto it2 = R2.begin();
    if(it2 != R2.end()){
      for (; it2 != R2.end(); ++it2) {
	if(it2[3].as<int>() != stoi(acc_id) && it2[4].as<int>() != stoi(acc_id))
	  continue;
	executed tmp = {it2[0].as<int>(), it2[1].as<double>(), it2[2].as<long>()};
	cancel_executed.push_back(tmp);
      }
    }

    // if no record found, then return 0
    if(cancel_canceled.empty() && cancel_executed.empty())
      return 0;
    
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    return 0;
  }
  return 1;
}

int Database::check_balance(string acc_id, int amount, double limit) {
  // if balance sufficient, return 1
  // if insufficient, return 0
  int total_money = amount * limit;
  try {
    work W(*C);
    string sql = "SELECT BALANCE from ACCOUNT WHERE ACCOUNT_ID=" + acc_id + ";";
    result R( W.exec(sql) );
    for (auto it = R.begin(); it != R.end(); ++it) {
      int balance = it[0].as<int>();
      if (balance < total_money) { // insufficient balance
        return 0;
      } else { // deduct total_money from balance
        int deducted_balance = balance - total_money;
        //nontransaction N2(*C);
        string update_balance_sql =\
            "UPDATE ACCOUNT set BALANCE=" + to_string(deducted_balance) +\
            " WHERE ACCOUNT_ID=" + acc_id + ";";
        //work W(*C);
        W.exec(update_balance_sql);
	
        W.commit();
        return 1;
      }
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    return 0;
  }
  return 1;
}

int Database::check_amount(string acc_id, int amount, string sym) {
  // if sufficient, then deduct and return 1
  // if insufficient, then renturn 0
  try {
    work W(*C);
    string sql = "SELECT AMOUNT from POSITION WHERE ACCOUNT_ID=" + acc_id +\
                 " AND SYM='" + sym + "';";
    result R( W.exec(sql) );
    
    auto it = R.begin();
    if (it != R.end()){
      if (amount < 0){
	int acc_amount = it[0].as<int>();
	if (acc_amount + amount < 0) { // insufficient amount
	  return 0;
	} else { // deduct sell_amount from account amount
	  int deducted_amount = acc_amount + amount;
	  string update_balance_sql =                                   \
	    "UPDATE POSITION set AMOUNT=" + to_string(deducted_amount) + \
	    " WHERE ACCOUNT_ID=" + acc_id + " AND SYM='" + sym + "';";
	  W.exec(update_balance_sql);
	  W.commit();
	  return 1;
	}
      }
    }
    else{
      cout << "no such position" << endl;
      return 0;
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    return 0;
  }
  return 1;
}

bool Database::is_acc_valid(string acc_id) {
  string sql;
  try {
    nontransaction N(*C);
    sql = "SELECT ACCOUNT_ID from ACCOUNT WHERE ACCOUNT_ID=" + acc_id + ";";
    result R(N.exec(sql));
    auto it = R.begin();
    if(it == R.end()){
      return false;
    }
    else{
      return true;
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
  }
  return true;
}
