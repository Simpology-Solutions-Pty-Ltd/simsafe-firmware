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
  if (getenv("GPIO_CHIP_NAME") == NULL) {
    cout << "GPIO_CHIP_NAME env variable required" << endl;
    exit(1);
  }
  if (getenv("CONTROLLER_SERIAL_NUMBER") == NULL) {
    cout << "CONTROLLER_SERIAL_NUMBER env variable required" << endl;
    exit(1);
  }
  cout << "Environment loaded!" << endl;
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
    for (HARDWARE_POSITIONS_TYPE i = 0; i < num_hardware_positions; i++) {
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

void AuthCodeRead(const char *auth_code, int length) {
  cout << "Auth code read: ";
  for (int i = 0; i < length; i++) {
    cout << auth_code[i];
  }
  cout << endl;

  auto conn = FetchConnection();
  vector<bool> output(num_hardware_positions);

  AuthCardScanned(&conn->conn, auth_code, length, &output);

  conn->in_use = false;

  cout << "Access received: ";
  for (HARDWARE_POSITIONS_TYPE i = 0; i < num_hardware_positions; i++) {
    if (output.at(i)) 
      cout << '1';
    else
      cout << '0';
  }
  cout << endl;

  SendWordToGPIO(&output);
  OpenGPIOOutput();
  this_thread::sleep_for(chrono::milliseconds(200));
  CloseGPIOOutput();

}

void *ReadSerialThreadTask(void *arg) {
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  
  int fd = *(int*)arg;

  char last_read_content[512] = {0};
  char buffer[512] = {0};
  int bytes_read, cursor_pos = 0;
  
  while (true) {
    if ((bytes_read = ReadFromSerialPort(fd, buffer, 512)) > 0) {
      memcpy(&last_read_content[cursor_pos], buffer, bytes_read);
      cursor_pos += bytes_read;
      
      if (cursor_pos < 512) {
        if (last_read_content[cursor_pos - 1] == '\n') {
          // Terminating char has been sent, fire 'event'
          AuthCodeRead(last_read_content, cursor_pos - 1);
          cursor_pos = 0;
        }
      } else {
        if (last_read_content[511] == '\n') {
          AuthCodeRead(last_read_content, 511);
        } else {
          AuthCodeRead(last_read_content, 512);
        }
        cursor_pos = 0;
      }
    }
    // cout << "Read" << endl;
    pthread_testcancel();
    this_thread::sleep_for(chrono::milliseconds(10));
  }

  return NULL;
}

void *TempWriteSerialReaderThreadTask(void *arg) {
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  
  int fd = *(int*)arg;

  while (true) {
    WriteToSerialPort(fd, "12345", 4);
    pthread_testcancel();
    this_thread::sleep_for(chrono::milliseconds(5000));
  }

  return NULL;
}

void HandleSignal(int signum) {
  try {
    cout << "\nKill signal received. Closing threads and exiting program..." << endl;
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
    cout << "Closing GPIO..." << endl;
    ResetGPIO();
    CloseGPIO();
    cout << "GPIO closed!" << endl;
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

  cout << "Opening GPIO..." << endl;

  if (OpenGPIOChip(getenv("GPIO_CHIP_NAME"))) {
    cout << "Could not open GPIO chip" << endl;
    // TODO: Decide what do to
    CloseConnectionPool();
    exit(1);
  }

  if (GetGPIOLines()) {
    cout << "Could not get GPIO lines" << endl;
    // TODO: Decide what do to
    CloseGPIOChipOnly();
    CloseConnectionPool();
    exit(1);
  }

  if (ConfigureGPIOChip()) {
    cout << "Could not configure GPIO lines" << endl;
    // TODO: Decide what do to
    CloseGPIO();
    CloseConnectionPool();
    exit(1);
  }

  ResetGPIO();

  ReadDipSwitchIntoGlobal();

  cout << "GPIO opened!" << endl;

  cout << "Cabinetid: " << cabinetid << endl << "Num positions hardware: " << num_hardware_positions << endl;

  if (!DoesCabinetPositionMatchHardwarePositionCount(&conn->conn)) {
    cout << "Cabinet does not contain the same amount of positions as dip switches are reporting" << endl;
    // TODO: Decide what do to
    CloseGPIO();
    CloseConnectionPool();
    exit(1);
  }
 
  cout << "Initialization complete\nProgram will now run for the rest of eternity, unless stopped o7" << endl;

  int fd = OpenSerialPort("/dev/ttyACM0");
  ConfigureSerialPort(fd, 9600);

  pthread_t temp;
  pthread_create(&temp, NULL, ReadSerialThreadTask, &fd);
  work_threads.push_back(temp);

  while (true) {
    this_thread::sleep_for(chrono::seconds(1));
    if (!IsHealthy(&conn->conn)) {
      cout << "Database connection lost. Reconnecting..." << endl;
      CloseConnectionPool();
      goto connect_db;
    }
  }
}
