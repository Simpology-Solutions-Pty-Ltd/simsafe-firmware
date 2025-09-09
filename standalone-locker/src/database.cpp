#include <iostream>
#include <pqxx/pqxx>
#include <unistd.h>
#include <thread>

using namespace std;
using namespace pqxx;

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

