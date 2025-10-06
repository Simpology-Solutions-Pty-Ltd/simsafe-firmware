#include <iostream>
#include <vector>
#include <inttypes.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <gpiod.hpp>
#include <string.h>

using namespace std;

u_int16_t num_hardware_positions = 0;
struct gpiod_chip *gpio_chip;
struct gpiod_line_request_config gpio_config;
struct gpiod_line_bulk gpio_lines;
unsigned int gpio_offsets[] = { 17, 27, 22, 23, 24 };
int gpio_values[] = { 1, 1, 0, 0, 0 };

#define GPIO_OE 0
#define GPIO_SRCLR 1
#define GPIO_SRCLK 2
#define GPIO_RCLK 3
#define GPIO_SER 4

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

int OpenGPIOChip(const char *name) {
  gpio_chip = gpiod_chip_open(name);
  if (gpio_chip == NULL) return -1;
  return 0;
}

int GetGPIOLines() {
  return gpiod_chip_get_lines(gpio_chip, gpio_offsets, 5, &gpio_lines);
}

int ConfigureGPIOChip() {
  memset(&gpio_config, 0, sizeof(gpio_config));
  gpio_config.consumer = "simsafe_firmware";
  gpio_config.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;
  gpio_config.flags = 0;

  return gpiod_line_request_bulk(&gpio_lines, &gpio_config, gpio_values);
}

void ResetGPIO() {
  gpio_values[GPIO_OE] = 1;
  gpio_values[GPIO_SRCLR] = 1;
  gpio_values[GPIO_SRCLK] = 0;
  gpio_values[GPIO_RCLK] = 0;
  gpio_values[GPIO_SER] = 0;
  gpiod_line_set_value_bulk(&gpio_lines, gpio_values);

  gpio_values[GPIO_SRCLR] = 0;
  gpio_values[GPIO_SRCLK] = 1;
  gpiod_line_set_value_bulk(&gpio_lines, gpio_values);

  gpio_values[GPIO_SRCLR] = 1;
  gpio_values[GPIO_SRCLK] = 0;
  gpio_values[GPIO_RCLK] = 1;
  gpiod_line_set_value_bulk(&gpio_lines, gpio_values);

  gpio_values[GPIO_RCLK] = 0;
  gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
}

int OpenGPIOOutput() {
  gpio_values[GPIO_OE] = 0;
  return gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
}

int CloseGPIOOutput() {
  gpio_values[GPIO_OE] = 1;
  return gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
}

void CloseGPIOChipOnly() {
  gpiod_chip_close(gpio_chip);
}

void CloseGPIO() {
  gpiod_line_release_bulk(&gpio_lines);
  gpiod_chip_close(gpio_chip);
}

void SendWordToGPIO(vector<bool> *values) {
  for (int i = num_hardware_positions - 1; i >= 0; i--) {
    if (values->at(i)) {
      gpio_values[GPIO_SER] = 1;
      gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
    } else {
      gpio_values[GPIO_SER] = 0;
    }

    gpio_values[GPIO_SRCLK] = 1;
    gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
    gpio_values[GPIO_SRCLK] = 0;
    gpio_values[GPIO_SER] = 0;
    gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
  }

  gpio_values[GPIO_RCLK] = 1;
  gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
  gpio_values[GPIO_RCLK] = 0;
  gpiod_line_set_value_bulk(&gpio_lines, gpio_values);
}