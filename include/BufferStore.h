#pragma once

#include <Arduino.h>

namespace buffer_store
{
  bool begin(bool purgeOnBoot);
  void appendLine(const String &line);
  bool prepareSyncSnapshot();
  String readSyncPayload();
  void completeSyncSnapshot();
  void discardOversizedBuffers();
}
