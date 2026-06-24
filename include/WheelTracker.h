#pragma once

#include <Arduino.h>

namespace wheel_tracker
{
  struct DashboardState
  {
    float currentSpeedKmh;
    float dailyDistanceM;
    float dailyRotations;
    bool sessionActive;
  };

  void begin();
  void updateDailyBoundary();
  void collectAndBufferSample(uint32_t nowMs);
  DashboardState dashboardState();
}
