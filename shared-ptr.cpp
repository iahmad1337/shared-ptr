#include "shared-ptr.h"

namespace sharedptr_details {
size_t control_block::get_strong() const {
  return strong_cnt;
}

void control_block::inc_strong() {
  strong_cnt++;
  weak_cnt++;
}

void control_block::dec_strong() {
  strong_cnt--;
  if (strong_cnt == 0) {
    delete_data();
  }
  dec_weak();
}

void control_block::inc_weak() {
  weak_cnt++;
}

void control_block::dec_weak() {
  weak_cnt--;
  if (weak_cnt == 0) {
    delete this; // call to the virtual destructor
  }
}
} // namespace sharedptr_details
