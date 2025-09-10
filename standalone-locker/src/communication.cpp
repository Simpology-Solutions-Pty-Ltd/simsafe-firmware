#include <iostream>
#include <vector>

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