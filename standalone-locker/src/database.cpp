#include <iostream>
#include <pqxx/pqxx>
#include <unistd.h>
#include <thread>
#include "communication.cpp"

using namespace pqxx;

typedef struct _db_connection {
  connection conn;
  bool in_use;
} db_connection;

unique_ptr<vector<db_connection>> _connections = make_unique<vector<db_connection>>(0);
#define DB_CONNECTION_COUNT 10
long cabinetid = 0;

void InitializeConnectionPools(void) noexcept(true) {
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

  bool connected = false;
  _connections.get()->clear();

  for (long unsigned int i = 0; i < DB_CONNECTION_COUNT; i++) {
    connected = false;
    while (!connected) {
      try {
        _connections.get()->push_back(db_connection {
          .conn = connection(connection_string),
          .in_use = false
        }); 
        connected = true;
      } catch (exception const &e) {
        cout << "Failed to connect to database: " << e.what() << "Retrying in 5 seconds..." << endl;
        this_thread::sleep_for(chrono::seconds(5));
      }
    }
  }

  cout << "Database connection successful!" << endl;
}

void CloseConnectionPool(void) noexcept(true) {
  for (long unsigned int i = 0; i < _connections.get()->size(); i++) {
    try {
      _connections.get()->at(i).conn.close();
    } catch (exception const &e) {}
  }
}

db_connection* FetchConnection(void) {
  for (long unsigned int i = 0; i < _connections.get()->size(); i++) {
    if (!_connections.get()->at(i).in_use) {
      _connections.get()->at(i).in_use = true;
      return &_connections.get()->at(i);
    }
  }

  return NULL;
}

bool DoesCabinetExist(connection *conn) noexcept(true) {
  if (conn == NULL) {
    return false;
  }


  try {
    work tx{*conn};
    tx.query_value<int>("select cabinetid from cabinet where controller_serialno = " + tx.quote(getenv("CONTROLLER_SERIAL_NUMBER")));
  } catch (exception const &e) {
    return false;
  }

  return true;
}

void ReadCabinetIdIntoGlobal(connection *conn) {
  if (conn == NULL) {
    return;
  }

  try {
    work tx{*conn};
    cabinetid = tx.query_value<long>("select cabinetid from cabinet where controller_serialno = " + tx.quote(getenv("CONTROLLER_SERIAL_NUMBER")));
  } catch (exception const &e) {}
}

bool DoesCabinetPositionMatchHardwarePositionCount(connection *conn) noexcept(true) {
  if (conn == NULL || num_hardware_positions < 1) {
    return false;
  }

  try {
    work tx{*conn};
    int count = tx.query_value<int>("select count(1) from cabinet c join position p on p.cabinetid = c.cabinetid where c.cabinetid = " + to_string(cabinetid));
    if (count != num_hardware_positions) return false;
  } catch (exception const &e) {
    return false;
  }

  return true;
}

void CreatePositionOpenedEvent(connection *conn, u_int16_t index) noexcept(false) {
  if (conn == NULL || index < 1) {
    return;
  }

  work tx{*conn};
  tx.exec("call \"eventInsertPositionOpened\"(" + tx.quote(getenv("CONTROLLER_SERIAL_NUMBER")) + "," + to_string(index) + ")");
  tx.commit();
}

void CreatePositionClosedEvent(connection *conn, u_int16_t index) noexcept(false) {
  if (conn == NULL || index < 1) {
    return;
  }

  work tx{*conn};
  tx.exec("call \"eventInsertPositionClosed\"(" + tx.quote(getenv("CONTROLLER_SERIAL_NUMBER")) + "," + to_string(index) + ")");
  tx.commit();
}

vector<bool> *AuthCardScanned(connection *conn, const char *auth_code, int length, vector<bool> *output) noexcept(true) {
  if (conn == NULL || output == NULL || length < 1) {
    return output;
  }

  char buffer[513] = {0};

  memcpy(buffer, auth_code, length);

  try {
    work tx{*conn};
    string access_string = tx.query_value<string>("select \"cardScanned\"(" + tx.quote(getenv("CONTROLLER_SERIAL_NUMBER")) + "," + tx.quote(buffer) + ")");
    tx.commit();
  
    if (access_string.empty()) {
      return output;
    }
  
    for (size_t i = 0; i < access_string.length(); i++) {
      output->at(i) = access_string[i] == '1';
    }
  } catch (exception const &e) {}


  return output;
}