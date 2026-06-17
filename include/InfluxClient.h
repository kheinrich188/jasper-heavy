#pragma once

#include <Arduino.h>

namespace influx_client
{
  bool configured();
  bool uploadPayload(const String &payload);
  String sanitizePayload(const String &payload, size_t &droppedLines);
}
