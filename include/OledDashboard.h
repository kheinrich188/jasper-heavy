#pragma once

namespace oled_dashboard
{
  void begin();
  void update(float currentSpeedKmh, float dailyDistanceM, float dailyRotations);
}
