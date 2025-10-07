#include <iostream>
#include <vector>
#include <inttypes.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <gpiod.hpp>
#include <string.h>

using namespace std;

#define HARDWARE_POSITIONS_TYPE u_int16_t
HARDWARE_POSITIONS_TYPE num_hardware_positions = 0;
struct gpiod_chip *gpio_chip;
struct gpiod_line_request_config gpio_config;
struct gpiod_line_bulk gpio_lines_output;
struct gpiod_line_bulk gpio_lines_input;
#define NUM_GPIO_OUTPUT 8
unsigned int gpio_output_offsets[] = { 17, 27, 22, 23, 24, 5, 16, 26 };
#define NUM_GPIO_INPUT 1
unsigned int gpio_input_offsets[] = { 6 };
int gpio_output_values[] = { 1, 1, 0, 0, 0, 0, 1, 1 };
int gpio_input_values[] = { 0 };

#define GPIO_OUTPUT_OE 0
#define GPIO_OUTPUT_SRCLR 1
#define GPIO_OUTPUT_SRCLK 2
#define GPIO_OUTPUT_RCLK 3
#define GPIO_OUTPUT_SER 4

#define GPIO_INPUT_CLK 5
#define GPIO_INPUT_CLR 6
#define GPIO_INPUT_LD 7
#define GPIO_INPUT_DATA 0

void ReadDipSwitchIntoGlobal(void) {
  // TODO: Implement
  num_hardware_positions = 8;
}

vector<bool>* FetchPositionStates(vector<bool> *states) {
  // TODO: Implement
  if (states->size() < num_hardware_positions) {
    for (HARDWARE_POSITIONS_TYPE i = 0, n = num_hardware_positions - states->size(); i < n; i++) {
      states->push_back(false);
    }
  }
  for (HARDWARE_POSITIONS_TYPE i = 0; i < num_hardware_positions; i++) {
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

void CloseSerialPort(int fd) {
  close(fd);
}

int OpenGPIOChip(const char *name) {
  gpio_chip = gpiod_chip_open(name);
  if (gpio_chip == NULL) return -1;
  return 0;
}

int GetGPIOOutputLines() {
  return gpiod_chip_get_lines(gpio_chip, gpio_output_offsets, NUM_GPIO_OUTPUT, &gpio_lines_output);
}

int GetGPIOInputLines() {
  return gpiod_chip_get_lines(gpio_chip, gpio_input_offsets, NUM_GPIO_INPUT, &gpio_lines_input);
}

int ConfigureGPIOChipOutput() {
  memset(&gpio_config, 0, sizeof(gpio_config));
  gpio_config.consumer = "simsafe_firmware";
  gpio_config.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;
  gpio_config.flags = 0;

  return gpiod_line_request_bulk(&gpio_lines_output, &gpio_config, gpio_output_values);
}

int ConfigureGPIOChipInput() {
  memset(&gpio_config, 0, sizeof(gpio_config));
  gpio_config.consumer = "simsafe_firmware";
  gpio_config.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
  gpio_config.flags = 0;

  return gpiod_line_request_bulk(&gpio_lines_input, &gpio_config, gpio_input_values);
}

void ResetGPIO() {
  gpio_output_values[GPIO_OUTPUT_OE] = 1;
  gpio_output_values[GPIO_OUTPUT_SRCLR] = 1;
  gpio_output_values[GPIO_OUTPUT_SRCLK] = 0;
  gpio_output_values[GPIO_OUTPUT_RCLK] = 0;
  gpio_output_values[GPIO_OUTPUT_SER] = 0;
  gpio_output_values[GPIO_INPUT_CLR] = 0;
  gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);

  gpio_output_values[GPIO_OUTPUT_SRCLR] = 0;
  gpio_output_values[GPIO_OUTPUT_SRCLK] = 1;
  gpio_output_values[GPIO_INPUT_CLR] = 1;
  gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);

  gpio_output_values[GPIO_OUTPUT_SRCLR] = 1;
  gpio_output_values[GPIO_OUTPUT_SRCLK] = 0;
  gpio_output_values[GPIO_OUTPUT_RCLK] = 1;
  gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);

  gpio_output_values[GPIO_OUTPUT_RCLK] = 0;
  gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
}

int OpenGPIOOutput() {
  gpio_output_values[GPIO_OUTPUT_OE] = 0;
  return gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
}

int CloseGPIOOutput() {
  gpio_output_values[GPIO_OUTPUT_OE] = 1;
  return gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
}

void CloseGPIOChipOnly() {
  gpiod_chip_close(gpio_chip);
}

void CloseGPIOOutputLines() {
  gpiod_line_release_bulk(&gpio_lines_output);
  gpiod_chip_close(gpio_chip);
}

void CloseGPIO() {
  gpiod_line_release_bulk(&gpio_lines_output);
  gpiod_line_release_bulk(&gpio_lines_input);
  gpiod_chip_close(gpio_chip);
}

void SendWordToGPIO(const vector<bool> *values) {
  try {
    for (HARDWARE_POSITIONS_TYPE i = num_hardware_positions; i > 0; i--) {
      if (values->at(i - 1)) {
        gpio_output_values[GPIO_OUTPUT_SER] = 1;
        gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
      } else {
        gpio_output_values[GPIO_OUTPUT_SER] = 0;
      }
  
      gpio_output_values[GPIO_OUTPUT_SRCLK] = 1;
      gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
      gpio_output_values[GPIO_OUTPUT_SRCLK] = 0;
      gpio_output_values[GPIO_OUTPUT_SER] = 0;
      gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    }
  
    gpio_output_values[GPIO_OUTPUT_RCLK] = 1;
    gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    gpio_output_values[GPIO_OUTPUT_RCLK] = 0;
    gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
  } catch (exception const *e) {
    cout << "Exception while writing GPIO: " << e->what() << endl;
  }
}

void ReadGPIO(vector<bool> *output) {
  try {
    gpio_output_values[GPIO_INPUT_CLR] = 0;
    gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    gpio_output_values[GPIO_INPUT_CLR] = 1;
    gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    gpio_output_values[GPIO_INPUT_LD] = 0;
    gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    gpio_output_values[GPIO_INPUT_CLK] = 1;
    gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    gpio_output_values[GPIO_INPUT_LD] = 1;
    gpio_output_values[GPIO_INPUT_CLK] = 0;
    gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    
    for (HARDWARE_POSITIONS_TYPE i = num_hardware_positions; i > 0; i--) {
      gpiod_line_get_value_bulk(&gpio_lines_input, gpio_input_values);
      output->at(i - 1) = gpio_input_values[GPIO_INPUT_DATA];
      gpio_output_values[GPIO_INPUT_CLK] = 1;
      gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
      gpio_output_values[GPIO_INPUT_CLK] = 0;
      gpiod_line_set_value_bulk(&gpio_lines_output, gpio_output_values);
    }
  } catch (exception const *e) {
    cout << "Exception while reading GPIO: " << e->what() << endl;
  }
}
