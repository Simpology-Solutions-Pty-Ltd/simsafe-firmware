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
pthread_t work_thread;

void LoadEnv() noexcept(true) {
  cout << "Loading environment..." << endl;
  try {
    dotenv::init();
  } catch (exception const &e) {
    cout << "Error opening env file: " << e.what() << endl;
    exit(1);
  }
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

unique_ptr<connection> ConnectDatabase() noexcept(true) {
  string connection_string = "host=";
  try {
    connection_string.append(getenv("DATABASE_HOST"));
    connection_string.append(" dbname=");
    connection_string.append(getenv("DATABASE_NAME"));
    connection_string.append(" user=");
    connection_string.append(getenv("DATABASE_USERNAME"));
    connection_string.append(" password=");
    connection_string.append(getenv("DATABASE_PASSWORD"));
  } catch (exception const &e) {
    cout << "Error loading environment variables: " << e.what() << "Connecting using default values" << endl;
    connection_string.clear();
    connection_string = "host=localhost dbname=simsafe user=postgres password=postgres";
  }

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

void *ExampleWorkerThreadTask(void *arg) {
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  while (true) {
    auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    while (chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start < 1000);
    pthread_testcancel();
    cout << "Worker thread tick" << endl;
  }
  return NULL;
}

void HandleSignal(int signum) {
  try {
    cout << "\nKill signal received. Closing threads and exiting program..." << std::endl;
    // Add any cleanup here
    cout << "Closing worker thread..." << endl;
    pthread_cancel(work_thread);
    pthread_join(work_thread, NULL);
    cout << "Worker thread closed!" << endl;
    if (conn) conn->close();
  } catch (exception const &e) {
    cout << "Error during shutdown: " << e.what() << "Exiting anyways..." << endl;
    exit(1);
  }
  exit(0);
}

bool IsHealthy() noexcept(true) {
  if (!conn || !conn->is_open()) {
    return false;
  }
  
  try {
    nontransaction ntxn(*conn.get(), "health_check");
    result result = ntxn.exec("SELECT version()");
    
    if (result.size() > 0) {
      return true;
    }
  } catch (exception const &e) {
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

  pthread_create(&work_thread, NULL, ExampleWorkerThreadTask, NULL);

  while (true) {
    this_thread::sleep_for(chrono::seconds(1));
    if (!IsHealthy()) {
      cout << "Database connection lost. Reconnecting..." << std::endl;
      try {
        conn->close();
      } catch (exception const &e) {
        cout << "Error when closing database connection: " << e.what() << "This does not interrupt reconnection attempt" << endl;
      }
      goto connect_db;
    }
  }
}