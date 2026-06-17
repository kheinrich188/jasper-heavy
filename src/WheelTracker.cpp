#include "WheelTracker.h"

#include <math.h>
#include <time.h>

#include "BufferStore.h"
#include "Config.h"
#include "InfluxClient.h"
#include "PulseCounter.h"
#include "WifiTime.h"

namespace
{
  float totalDistanceM = 0.0f;
  float totalRotations = 0.0f;
  float dailyDistanceM = 0.0f;
  float dailyRotations = 0.0f;
  float currentSpeedKmh = 0.0f;
  int32_t currentDayKey = -1;
  uint32_t lastSampleMs = 0;
  uint32_t lastHeartbeatMs = 0;
  bool sessionActive = false;
  uint32_t sessionId = 0;
  uint32_t sessionStartMs = 0;
  uint32_t sessionLastPulseMs = 0;
  float sessionDistanceM = 0.0f;
  float sessionRotations = 0.0f;
  float sessionMaxKmh = 0.0f;

  time_t currentUnixTimestamp()
  {
    time_t unixTs = time(nullptr);
    if (unixTs < 1700000000)
    {
      return 0;
    }
    return unixTs;
  }

  void appendTimestampIfKnown(String &line, time_t unixTs)
  {
    if (unixTs >= 1700000000)
    {
      line += " ";
      line += String(static_cast<unsigned long>(unixTs));
    }
  }

  void bufferLineIfEnabled(const String &line)
  {
    if (influx_client::configured())
    {
      buffer_store::appendLine(line);
    }
  }
} // namespace

namespace wheel_tracker
{
  void begin()
  {
    pulse_counter::begin(config::HALL_SENSOR_PIN);
    lastSampleMs = millis();
  }

  void updateDailyBoundary()
  {
    const int32_t dayKey = wifi_time::localDayKey(time(nullptr));
    if (dayKey < 0)
    {
      return;
    }

    if (currentDayKey < 0)
    {
      currentDayKey = dayKey;
      return;
    }

    if (dayKey != currentDayKey)
    {
      currentDayKey = dayKey;
      dailyDistanceM = 0.0f;
      dailyRotations = 0.0f;
      Serial.println("Tageszaehler: Neuer lokaler Tag, Tageswerte zurueckgesetzt.");
    }
  }

  void collectAndBufferSample(uint32_t nowMs)
  {
    const uint32_t elapsedMs = nowMs - lastSampleMs;
    if (elapsedMs == 0)
    {
      return;
    }
    lastSampleMs = nowMs;

    const uint32_t pulses = pulse_counter::take();
    const float rotations = static_cast<float>(pulses) / static_cast<float>(config::MAGNETS_PER_ROTATION);
    const float elapsedS = static_cast<float>(elapsedMs) / 1000.0f;
    const float rpm = (elapsedS > 0.0f) ? (rotations * 60.0f / elapsedS) : 0.0f;
    const float speedMps = (elapsedS > 0.0f) ? (rotations * config::INNER_CIRCUMFERENCE_M / elapsedS) : 0.0f;
    const float speedKmh = speedMps * 3.6f;
    const time_t unixTs = currentUnixTimestamp();
    currentSpeedKmh = speedKmh;

    if (pulses > 0)
    {
      if (!sessionActive)
      {
        sessionActive = true;
        sessionId += 1;
        sessionStartMs = nowMs;
        sessionDistanceM = 0.0f;
        sessionRotations = 0.0f;
        sessionMaxKmh = 0.0f;
      }
      sessionLastPulseMs = nowMs;
      sessionRotations += rotations;
      sessionDistanceM += rotations * config::INNER_CIRCUMFERENCE_M;
      sessionMaxKmh = max(sessionMaxKmh, speedKmh);

      lastHeartbeatMs = nowMs;

      totalRotations += rotations;
      totalDistanceM += rotations * config::INNER_CIRCUMFERENCE_M;
      dailyRotations += rotations;
      dailyDistanceM += rotations * config::INNER_CIRCUMFERENCE_M;

      const uint32_t sessionDurationS = (nowMs - sessionStartMs) / 1000U;
      String line = "cat_wheel,device=heltec_v3,direction=clockwise ";
      line += "pulses=" + String(pulses) + "i";
      line += ",rotations=" + String(rotations, 4);
      line += ",rpm=" + String(rpm, 2);
      line += ",speed_kmh=" + String(speedKmh, 2);
      line += ",distance_total_m=" + String(totalDistanceM, 3);
      line += ",rotations_total=" + String(totalRotations, 3);
      line += ",daily_distance_m=" + String(dailyDistanceM, 3);
      line += ",daily_rotations=" + String(lroundf(dailyRotations)) + "i";
      line += ",session_id=" + String(sessionId) + "i";
      line += ",session_active=1i";
      line += ",session_duration_s=" + String(sessionDurationS) + "i";
      line += ",session_distance_m=" + String(sessionDistanceM, 3);
      line += ",session_rotations=" + String(sessionRotations, 3);
      line += ",session_max_kmh=" + String(sessionMaxKmh, 2);
      line += ",uptime_ms=" + String(nowMs) + "i";
      line += ",unix_ts=" + String(static_cast<unsigned long>(unixTs)) + "i";
      appendTimestampIfKnown(line, unixTs);
      bufferLineIfEnabled(line);

      Serial.printf("Sample: pulses=%lu rpm=%.2f speed=%.1f km/h session=%lu/%lus total=%.2f m ts=%lu\n",
                    static_cast<unsigned long>(pulses), rpm, speedKmh, static_cast<unsigned long>(sessionId),
                    static_cast<unsigned long>(sessionDurationS), totalDistanceM, static_cast<unsigned long>(unixTs));
      return;
    }

    if (sessionActive && (nowMs - sessionLastPulseMs >= config::SESSION_IDLE_TIMEOUT_MS))
    {
      const uint32_t sessionDurationS = (sessionLastPulseMs - sessionStartMs) / 1000U;
      String endLine = "cat_wheel,device=heltec_v3,direction=clockwise ";
      endLine += "session_id=" + String(sessionId) + "i";
      endLine += ",session_active=0i";
      endLine += ",session_ended=1i";
      endLine += ",session_duration_s=" + String(sessionDurationS) + "i";
      endLine += ",session_distance_m=" + String(sessionDistanceM, 3);
      endLine += ",session_rotations=" + String(sessionRotations, 3);
      endLine += ",session_max_kmh=" + String(sessionMaxKmh, 2);
      endLine += ",daily_distance_m=" + String(dailyDistanceM, 3);
      endLine += ",daily_rotations=" + String(lroundf(dailyRotations)) + "i";
      endLine += ",distance_total_m=" + String(totalDistanceM, 3);
      endLine += ",uptime_ms=" + String(nowMs) + "i";
      endLine += ",unix_ts=" + String(static_cast<unsigned long>(unixTs)) + "i";
      appendTimestampIfKnown(endLine, unixTs);
      bufferLineIfEnabled(endLine);

      Serial.printf("Session Ende: id=%lu dauer=%lus dist=%.2f m max=%.1f km/h\n",
                    static_cast<unsigned long>(sessionId), static_cast<unsigned long>(sessionDurationS),
                    sessionDistanceM, sessionMaxKmh);

      sessionActive = false;
      sessionDistanceM = 0.0f;
      sessionRotations = 0.0f;
      sessionMaxKmh = 0.0f;
    }

    if (nowMs - lastHeartbeatMs < config::HEARTBEAT_INTERVAL_MS)
    {
      Serial.printf("Sample: idle speed=%.1f km/h total=%.2f m ts=%lu\n", speedKmh, totalDistanceM,
                    static_cast<unsigned long>(unixTs));
      return;
    }

    lastHeartbeatMs = nowMs;
    String heartbeat = "cat_wheel,device=heltec_v3,direction=clockwise ";
    heartbeat += "heartbeat=1i";
    heartbeat += ",speed_kmh=" + String(speedKmh, 2);
    heartbeat += ",distance_total_m=" + String(totalDistanceM, 3);
    heartbeat += ",daily_distance_m=" + String(dailyDistanceM, 3);
    heartbeat += ",daily_rotations=" + String(lroundf(dailyRotations)) + "i";
    heartbeat += ",session_active=" + String(sessionActive ? 1 : 0) + "i";
    heartbeat += ",session_id=" + String(sessionId) + "i";
    heartbeat += ",uptime_ms=" + String(nowMs) + "i";
    heartbeat += ",unix_ts=" + String(static_cast<unsigned long>(unixTs)) + "i";
    appendTimestampIfKnown(heartbeat, unixTs);
    bufferLineIfEnabled(heartbeat);

    Serial.printf("Sample: heartbeat speed=%.1f km/h total=%.2f m ts=%lu\n", speedKmh, totalDistanceM,
                  static_cast<unsigned long>(unixTs));
  }

  DashboardState dashboardState()
  {
    return DashboardState{
        currentSpeedKmh,
        dailyDistanceM,
        dailyRotations,
    };
  }
} // namespace wheel_tracker
