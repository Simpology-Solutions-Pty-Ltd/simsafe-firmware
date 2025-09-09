#include <iostream>
#include <pqxx/pqxx>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include "../include/dotenv/dotenv.h"
#include "database.cpp"

using namespace std;
using namespace pqxx;

unique_ptr<connection> conn;
vector<pthread_t> work_threads;

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

void *ExampleWorkerThreadTask(void *arg) {
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  while (true) {
    auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    while (chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count() - start < 1000);
    pthread_testcancel();
    cout << "Worker thread " << *(int*)arg << " tick " << start << endl;
  }
  return NULL;
}

void HandleSignal(int signum) {
  try {
    cout << "\nKill signal received. Closing threads and exiting program..." << std::endl;
    // Add any cleanup here
    cout << "Closing worker threads..." << endl;
    
    for (auto thread : work_threads) {
      pthread_cancel(thread);
    }
    for (auto thread : work_threads) {
      pthread_join(thread, NULL);
    }
    
    cout << "Worker threads closed!" << endl;
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

  auto cores_available = sysconf(_SC_NPROCESSORS_ONLN);

  cout << "Firmware initializing\nCores available: " << cores_available  << endl;

  LoadEnv();

  connect_db:
  conn = ConnectDatabase();
 
  cout << "Initialization complete\nProgram will now run for the rest of eternity, unless stopped o7" << endl;

  for (int i = 0; i < cores_available; i++) {
    pthread_t temp;
    int num = i;
    pthread_create(&temp, NULL, ExampleWorkerThreadTask, &num);
    work_threads.push_back(temp);
  }

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