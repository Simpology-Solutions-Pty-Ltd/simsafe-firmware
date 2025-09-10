#include <iostream>
#include <pqxx/pqxx>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include "../include/dotenv/dotenv.h"
#include "database.cpp"

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
  if (getenv("CONTROLLER_SERIAL_NUMBER") == NULL) {
    cout << "CONTROLLER_SERIAL_NUMBER env variable required" << endl;
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
    cout << "Worker thread tick " << start << endl;
  }
  return NULL;
}

void *PositionOpenedClosedEventsThreadTask(void *arg) {
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  
  auto current_states = make_unique<vector<bool>>(num_hardware_positions);
  auto next_states = make_unique<vector<bool>>(num_hardware_positions);

  FetchPositionStates(current_states.get());

  for (auto state : *current_states.get()) {
    cout << "Starting state: " << state << endl;
  }

  db_connection *conn;
  
  while (true) {
    this_thread::sleep_for(chrono::milliseconds(1000));
    pthread_testcancel();
    
    conn = FetchConnection();
    
    FetchPositionStates(next_states.get());
    for (int i = 0; i < num_hardware_positions; i++) {
      if (next_states->at(i) && !current_states->at(i)) {
        CreatePositionOpenedEvent(&conn->conn, i + 1);
      } else if (!next_states->at(i) && current_states->at(i)) {
        CreatePositionClosedEvent(&conn->conn, i + 1);
      }
    }

    current_states.swap(next_states);

    for (auto state : *current_states.get()) {
      cout << "Next state: " << state << endl;
    }

    conn->in_use = false;
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
    CloseConnectionPool();
  } catch (exception const &e) {
    cout << "Error during shutdown: " << e.what() << "Exiting anyways..." << endl;
    exit(1);
  }
  exit(0);
}

bool IsHealthy(connection *conn) noexcept(true) {
  if (!conn || !conn->is_open()) {
    return false;
  }
  
  try {
    nontransaction ntxn(*conn, "health_check");
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
  InitializeConnectionPools();

  auto conn = FetchConnection();
  
  if (!DoesCabinetExist(&conn->conn)) {
    cout << "Cabinet does not exist in database" << endl;
    // TODO: Need to decide what to do, make new cabinet? Exit?
  }

  ReadCabinetIdIntoGlobal(&conn->conn);
  ReadDipSwitchIntoGlobal();

  cout << "Cabinetid: " << cabinetid << endl << "Num positions hardware: " << num_hardware_positions << endl;

  if (!DoesCabinetPositionMatchHardwarePositionCount(&conn->conn)) {
    cout << "Cabinet does not contain the same amount of positions as dip switches are reporting" << endl;
    // TODO: Decide what do to
    CloseConnectionPool();
    exit(1);
  }
 
  cout << "Initialization complete\nProgram will now run for the rest of eternity, unless stopped o7" << endl;

  pthread_t temp;
  pthread_create(&temp, NULL, ExampleWorkerThreadTask, NULL);
  work_threads.push_back(temp);
  pthread_create(&temp, NULL, PositionOpenedClosedEventsThreadTask, NULL);
  work_threads.push_back(temp);

  while (true) {
    this_thread::sleep_for(chrono::seconds(1));
    if (!IsHealthy(&conn->conn)) {
      cout << "Database connection lost. Reconnecting..." << std::endl;
      CloseConnectionPool();
      goto connect_db;
    }
  }
}