#include <iostream>
#include <pqxx/pqxx>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include "../include/dotenv/dotenv.h"

using namespace std;
using namespace pqxx;

unique_ptr<connection> conn;

void LoadEnv() {
  cout << "Loading environment..." << endl;
  dotenv::init();
  if (getenv("DATABASE_NAME") == NULL) {
    cout << "DATABASE_NAME env variable required" << endl;
    exit(1);
  }
  if (getenv("DATABASE_USERNAME") == NULL) {
    cout << "DATABASE_USERNAME env variable required" << endl;
    exit(1);
  }
  if (getenv("DATABASE_PASSWORD") == NULL) {
    cout << "DATABASE_PASSWORD env variable required" << endl;
    exit(1);
  }
  if (getenv("DATABASE_HOST") == NULL) {
    cout << "DATABASE_HOST env variable required" << endl;
    exit(1);
  }
  cout << "Environment loaded!" << endl;
}

unique_ptr<connection> ConnectDatabase() {
  string connection_string = "host=";
  connection_string.append(getenv("DATABASE_HOST"));
  connection_string.append(" dbname=");
  connection_string.append(getenv("DATABASE_NAME"));
  connection_string.append(" user=");
  connection_string.append(getenv("DATABASE_USERNAME"));
  connection_string.append(" password=");
  connection_string.append(getenv("DATABASE_PASSWORD"));

  cout << "Connecting to database..." << endl;

  unique_ptr<connection> c;

  while (c == NULL) {
    try {
      c = make_unique<connection>(connection_string);
    } catch (exception const &e) {
      cout << "Failed to connect to database: " << e.what() << "Retrying in 5 seconds..." << endl;
      this_thread::sleep_for(chrono::seconds(5));
    }
  }

  cout << "Database connection successful!" << endl;

  return c;
}

void HandleSignal(int signum) {
  cout << "\nKill signal received. Exiting program..." << std::endl;
  // Add any cleanup here
  if (conn) conn->close();
  exit(0);
}

bool IsHealthy() {
  if (!conn || !conn->is_open()) {
    return false;
  }
  
  try {
    nontransaction ntxn(*conn.get(), "health_check");
    result result = ntxn.exec("SELECT version()");
    
    if (result.size() > 0) {
      return true;
    }
  } catch (const pqxx::broken_connection& e) {
    return false;
  } catch (const pqxx::sql_error& e) {
    return false;
  } catch (const std::exception& e) {
    return false;
  }
  
  return false;
}

int main() {
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);

  cout << "Firmware initializing" << endl;

  LoadEnv();

  connect_db:
  conn = ConnectDatabase();
 
  cout << "Initialization complete\nProgram will now run for the rest of eternity, unless stopped o7" << endl;

  while (true) {
    this_thread::sleep_for(chrono::seconds(1));
    if (!IsHealthy()) {
      cout << "Database connection lost. Reconnecting..." << std::endl;
      conn->close();
      goto connect_db;
    }
  }
}