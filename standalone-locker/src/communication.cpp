#include <iostream>
#include <vector>
#include <inttypes.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

u_int16_t num_hardware_positions = 0;

void ReadDipSwitchIntoGlobal(void) {
  // TODO: Implement
  num_hardware_positions = 4;
}

vector<bool>* FetchPositionStates(vector<bool> *states) {
  // TODO: Implement
  if (states->size() < num_hardware_positions) {
    for (int i = 0, n = num_hardware_positions - states->size(); i < n; i++) {
      states->push_back(false);
    }
  }
  for (int i = 0; i < num_hardware_positions; i++) {
    states->at(i) = rand() > (INT32_MAX / 2);
  }
  return states;
}

int OpenSerialPort(const char* portname) {
  int fd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    cerr << "Error opening " << portname << endl;
    return -1;
  }
  return fd;
}

bool ConfigureSerialPort(int fd, int speed) {
  struct termios tty;
  if (tcgetattr(fd, &tty) != 0) {
    cerr << "Error from tcgetattr" << endl;
    return false;
  }

  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  // Disable canonical mode and echo
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

  // Set raw input mode
  tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

  // Raw output
  tty.c_oflag &= ~OPOST;

  // Set minimum bytes to read (0 = don't wait for specific count)
  tty.c_cc[VMIN] = 0;
  // Set timeout in deciseconds (1 = 100ms, 0 = non-blocking)
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    cerr << "Error from tcsetattr" << endl;
    return false;
  }

  tcflush(fd, TCIOFLUSH);

  return true;
}

int ReadFromSerialPort(int fd, char* buffer, size_t size) {
  return read(fd, buffer, size);
}

int WriteToSerialPort(int fd, const char* buffer, size_t size) {
  cout << "Write: ";
  for (size_t i = 0; i < size; i++) {
    cout << (int)buffer[i];
  }
  cout << endl;
  return write(fd, buffer, size);
}

void closeSerialPort(int fd) {
  close(fd);
}