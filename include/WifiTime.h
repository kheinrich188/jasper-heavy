#pragma once

#include <Arduino.h>
#include <time.h>

namespace wifi_time
{
  void connectWifiIfNeeded();
  void initTimeIfNeeded();
  bool online();
  bool initialized();
  int32_t localDayKey(time_t unixTs);
}
