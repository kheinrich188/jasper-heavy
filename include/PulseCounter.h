#pragma once

#include <Arduino.h>

namespace pulse_counter
{
  void begin(int pin);
  uint32_t take();
  uint32_t pending();
}
