#include <Arduino.h>

#include "BufferStore.h"
#include "Config.h"
#include "InfluxClient.h"
#include "OledDashboard.h"
#include "WheelTracker.h"
#include "WifiTime.h"

namespace
{
  uint32_t lastSampleMs = 0;
  uint32_t lastSyncMs = 0;
  uint32_t lastDisplayMs = 0;
  volatile bool syncTaskRunning = false;
  portMUX_TYPE syncTaskMux = portMUX_INITIALIZER_UNLOCKED;

  bool tryStartSyncTask()
  {
    bool started = false;
    portENTER_CRITICAL(&syncTaskMux);
    if (!syncTaskRunning)
    {
      syncTaskRunning = true;
      started = true;
    }
    portEXIT_CRITICAL(&syncTaskMux);
    return started;
  }

  void finishSyncTask()
  {
    portENTER_CRITICAL(&syncTaskMux);
    syncTaskRunning = false;
    portEXIT_CRITICAL(&syncTaskMux);
  }

  void syncBufferedDataIfPossible()
  {
    if (!influx_client::configured())
    {
      return;
    }

    buffer_store::discardOversizedBuffers();
    if (!buffer_store::prepareSyncSnapshot())
    {
      return;
    }

    const String payload = buffer_store::readSyncPayload();
    if (payload.length() == 0)
    {
      buffer_store::completeSyncSnapshot();
      return;
    }

    size_t droppedLines = 0;
    const String sanitizedPayload = influx_client::sanitizePayload(payload, droppedLines);
    if (sanitizedPayload.length() == 0)
    {
      buffer_store::completeSyncSnapshot();
      if (droppedLines > 0)
      {
        Serial.printf("Sync: %u ungueltige Buffer-Zeilen verworfen.\n", static_cast<unsigned>(droppedLines));
      }
      return;
    }

    if (influx_client::uploadPayload(sanitizedPayload))
    {
      buffer_store::completeSyncSnapshot();
      if (droppedLines > 0)
      {
        Serial.printf("Sync: %u ungueltige Buffer-Zeilen verworfen.\n", static_cast<unsigned>(droppedLines));
      }
    }
  }

  void runSyncTask(void *parameter)
  {
    (void)parameter;
    syncBufferedDataIfPossible();
    finishSyncTask();
    vTaskDelete(nullptr);
  }
} // namespace

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("Cat Wheel Tracker startet...");
  Serial.printf("Sensor-Pins Kandidaten: %d / %d / %d\n", config::HALL_SENSOR_PIN, config::ALT_SENSOR_PIN_2,
                config::ALT_SENSOR_PIN_3);
  Serial.printf("Umfang innen=%.3f m, aussen=%.3f m\n", config::INNER_CIRCUMFERENCE_M,
                config::OUTER_CIRCUMFERENCE_M);

  buffer_store::begin(config::PURGE_BUFFER_ON_BOOT);
  oled_dashboard::begin();

  if (influx_client::configured())
  {
    Serial.println("Telemetry: Influx Upload aktiviert.");
  }
  else
  {
    Serial.println("Telemetry: Display-only Modus (Influx nicht konfiguriert).");
  }

  wheel_tracker::begin();
  wifi_time::connectWifiIfNeeded();
  wifi_time::initTimeIfNeeded();

  lastSampleMs = millis();
  lastSyncMs = millis();
  lastDisplayMs = millis();
}

void loop()
{
  wifi_time::connectWifiIfNeeded();
  wifi_time::initTimeIfNeeded();
  wheel_tracker::updateDailyBoundary();

  const uint32_t nowMs = millis();
  if (nowMs - lastSampleMs >= config::SAMPLE_INTERVAL_MS)
  {
    lastSampleMs = nowMs;
    wheel_tracker::collectAndBufferSample(nowMs);
  }

  if (nowMs - lastDisplayMs >= config::DISPLAY_INTERVAL_MS)
  {
    lastDisplayMs = nowMs;
    const wheel_tracker::DashboardState state = wheel_tracker::dashboardState();
    oled_dashboard::update(state.currentSpeedKmh, state.dailyDistanceM, state.dailyRotations, wifi_time::online());
  }

  if (nowMs - lastSyncMs >= config::SYNC_INTERVAL_MS)
  {
    lastSyncMs = nowMs;
    if (influx_client::configured() && tryStartSyncTask())
    {
      const BaseType_t created = xTaskCreatePinnedToCore(
          runSyncTask,
          "sync-task",
          8192,
          nullptr,
          1,
          nullptr,
          1);
      if (created != pdPASS)
      {
        finishSyncTask();
        Serial.println("Sync: Task konnte nicht gestartet werden.");
      }
    }
  }
}
