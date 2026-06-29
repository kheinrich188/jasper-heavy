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
  uint32_t dailyZoomies = 0;
  float dailyZoomiesIndex = 0.0f;
  float currentSpeedKmh = 0.0f;
  int32_t currentDayKey = -1;
  time_t nextDailyResetTs = 0;
  uint32_t lastSampleMs = 0;
  uint32_t lastHeartbeatMs = 0;
  uint32_t lastActivityMs = 0;
  bool sessionActive = false;
  bool sessionConfirmed = false;
  uint32_t sessionId = 0;
  uint32_t sessionStartMs = 0;
  uint32_t sessionLastPulseMs = 0;
  uint32_t sessionPulseCount = 0;
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

  bool isZoomiesSession(uint32_t durationS)
  {
    return durationS > 0 && durationS <= config::ZOOMIES_MAX_DURATION_S &&
           sessionMaxKmh >= config::ZOOMIES_MIN_MAX_SPEED_KMH &&
           sessionDistanceM >= config::ZOOMIES_MIN_DISTANCE_M;
  }

  float zoomiesScore(uint32_t durationS)
  {
    const float safeDurationS = static_cast<float>(durationS > 0 ? durationS : 1);
    return (sessionMaxKmh * sessionDistanceM) / safeDurationS;
  }

  uint32_t inactivityDurationS(uint32_t nowMs)
  {
    if (lastActivityMs == 0)
    {
      return 0;
    }
    return (nowMs - lastActivityMs) / 1000U;
  }

  bool inactivityWarningActive(uint32_t nowMs)
  {
    return lastActivityMs > 0 && nowMs - lastActivityMs >= config::INACTIVITY_WARNING_MS;
  }

  time_t nextLocalMidnight(time_t unixTs)
  {
    struct tm localTime;
    localtime_r(&unixTs, &localTime);
    localTime.tm_hour = 0;
    localTime.tm_min = 0;
    localTime.tm_sec = 0;
    localTime.tm_mday += 1;
    localTime.tm_isdst = -1;
    return mktime(&localTime);
  }

  void resetDailyCounters()
  {
    dailyDistanceM = 0.0f;
    dailyRotations = 0.0f;
    dailyZoomies = 0;
    dailyZoomiesIndex = 0.0f;
    sessionPulseCount = 0;
  }

  void updateDailySchedule(time_t unixTs)
  {
    currentDayKey = wifi_time::localDayKey(unixTs);
    nextDailyResetTs = nextLocalMidnight(unixTs);
  }

  void resetSession()
  {
    sessionActive = false;
    sessionConfirmed = false;
    sessionStartMs = 0;
    sessionLastPulseMs = 0;
    sessionPulseCount = 0;
    sessionDistanceM = 0.0f;
    sessionRotations = 0.0f;
    sessionMaxKmh = 0.0f;
  }

  bool sessionMeetsMinimum(uint32_t nowMs)
  {
    return nowMs - sessionStartMs >= config::MIN_SESSION_DURATION_MS &&
           sessionPulseCount >= config::MIN_SESSION_PULSES;
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
    const time_t now = time(nullptr);
    const int32_t dayKey = wifi_time::localDayKey(now);
    if (dayKey < 0)
    {
      return;
    }

    if (currentDayKey < 0)
    {
      updateDailySchedule(now);
      return;
    }

    if (dayKey != currentDayKey || (nextDailyResetTs > 0 && now >= nextDailyResetTs))
    {
      resetDailyCounters();
      updateDailySchedule(now);
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
        sessionConfirmed = false;
        sessionStartMs = nowMs;
        sessionPulseCount = 0;
        sessionDistanceM = 0.0f;
        sessionRotations = 0.0f;
        sessionMaxKmh = 0.0f;
      }
      sessionLastPulseMs = nowMs;
      sessionRotations += rotations;
      sessionDistanceM += rotations * config::INNER_CIRCUMFERENCE_M;
      sessionMaxKmh = max(sessionMaxKmh, speedKmh);

      lastHeartbeatMs = nowMs;

      const uint32_t completedRotationsBefore = sessionPulseCount / config::MAGNETS_PER_ROTATION;
      sessionPulseCount += pulses;
      const uint32_t completedRotationsAfter = sessionPulseCount / config::MAGNETS_PER_ROTATION;
      const uint32_t completedRotations = completedRotationsAfter - completedRotationsBefore;

      bool emitSample = sessionConfirmed;
      uint32_t telemetryPulses = pulses;
      float telemetryRotations = rotations;
      uint32_t telemetryCompletedRotations = completedRotations;

      if (!sessionConfirmed && sessionMeetsMinimum(nowMs))
      {
        sessionConfirmed = true;
        sessionId += 1;
        emitSample = true;
        telemetryPulses = sessionPulseCount;
        telemetryRotations = sessionRotations;
        telemetryCompletedRotations = completedRotationsAfter;
      }

      if (!emitSample)
      {
        return;
      }

      lastActivityMs = nowMs;
      totalRotations += telemetryRotations;
      totalDistanceM += telemetryRotations * config::INNER_CIRCUMFERENCE_M;
      dailyRotations += static_cast<float>(telemetryCompletedRotations);
      dailyDistanceM += telemetryRotations * config::INNER_CIRCUMFERENCE_M;

      const uint32_t sessionDurationS = (nowMs - sessionStartMs) / 1000U;
      String line = "cat_wheel,device=heltec_v3,direction=clockwise ";
      line += "pulses=" + String(telemetryPulses) + "i";
      line += ",rotations=" + String(telemetryRotations, 4);
      line += ",rpm=" + String(rpm, 2);
      line += ",speed_kmh=" + String(speedKmh, 2);
      line += ",distance_total_m=" + String(totalDistanceM, 3);
      line += ",rotations_total=" + String(totalRotations, 3);
      line += ",daily_distance_m=" + String(dailyDistanceM, 3);
      line += ",daily_rotations=" + String(dailyRotations, 3);
      line += ",daily_zoomies=" + String(dailyZoomies) + "i";
      line += ",daily_zoomies_index=" + String(dailyZoomiesIndex, 3);
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

      if (config::VERBOSE_SERIAL_LOGS)
      {
        Serial.printf("Sample: pulses=%lu rpm=%.2f speed=%.1f km/h session=%lu/%lus total=%.2f m ts=%lu\n",
                      static_cast<unsigned long>(telemetryPulses), rpm, speedKmh, static_cast<unsigned long>(sessionId),
                      static_cast<unsigned long>(sessionDurationS), totalDistanceM, static_cast<unsigned long>(unixTs));
      }
      return;
    }

    if (sessionActive && (nowMs - sessionLastPulseMs >= config::SESSION_IDLE_TIMEOUT_MS))
    {
      if (!sessionConfirmed)
      {
        if (config::VERBOSE_SERIAL_LOGS)
        {
          Serial.printf("Session verworfen: dauer=%lums pulses=%lu\n",
                        static_cast<unsigned long>(sessionLastPulseMs - sessionStartMs),
                        static_cast<unsigned long>(sessionPulseCount));
        }
        resetSession();
        return;
      }

      const uint32_t sessionDurationS = (sessionLastPulseMs - sessionStartMs) / 1000U;
      const bool zoomiesSession = isZoomiesSession(sessionDurationS);
      const float sessionZoomiesScore = zoomiesSession ? zoomiesScore(sessionDurationS) : 0.0f;
      if (zoomiesSession)
      {
        dailyZoomies += 1;
        dailyZoomiesIndex += sessionZoomiesScore;
      }

      String endLine = "cat_wheel,device=heltec_v3,direction=clockwise ";
      endLine += "session_id=" + String(sessionId) + "i";
      endLine += ",session_active=0i";
      endLine += ",session_ended=1i";
      endLine += ",zoomies=" + String(zoomiesSession ? 1 : 0) + "i";
      endLine += ",zoomies_score=" + String(sessionZoomiesScore, 3);
      endLine += ",daily_zoomies=" + String(dailyZoomies) + "i";
      endLine += ",daily_zoomies_index=" + String(dailyZoomiesIndex, 3);
      endLine += ",session_duration_s=" + String(sessionDurationS) + "i";
      endLine += ",session_distance_m=" + String(sessionDistanceM, 3);
      endLine += ",session_rotations=" + String(sessionRotations, 3);
      endLine += ",session_max_kmh=" + String(sessionMaxKmh, 2);
      endLine += ",daily_distance_m=" + String(dailyDistanceM, 3);
      endLine += ",daily_rotations=" + String(dailyRotations, 3);
      endLine += ",distance_total_m=" + String(totalDistanceM, 3);
      endLine += ",uptime_ms=" + String(nowMs) + "i";
      endLine += ",unix_ts=" + String(static_cast<unsigned long>(unixTs)) + "i";
      appendTimestampIfKnown(endLine, unixTs);
      bufferLineIfEnabled(endLine);

      if (config::VERBOSE_SERIAL_LOGS)
      {
        Serial.printf("Session Ende: id=%lu dauer=%lus dist=%.2f m max=%.1f km/h zoomies=%s\n",
                      static_cast<unsigned long>(sessionId), static_cast<unsigned long>(sessionDurationS),
                      sessionDistanceM, sessionMaxKmh, zoomiesSession ? "ja" : "nein");
      }

      resetSession();
    }

    if (nowMs - lastHeartbeatMs < config::HEARTBEAT_INTERVAL_MS)
    {
      return;
    }

    lastHeartbeatMs = nowMs;
    String heartbeat = "cat_wheel,device=heltec_v3,direction=clockwise ";
    heartbeat += "heartbeat=1i";
    heartbeat += ",speed_kmh=" + String(speedKmh, 2);
    heartbeat += ",distance_total_m=" + String(totalDistanceM, 3);
    heartbeat += ",daily_distance_m=" + String(dailyDistanceM, 3);
    heartbeat += ",daily_rotations=" + String(dailyRotations, 3);
    heartbeat += ",daily_zoomies=" + String(dailyZoomies) + "i";
    heartbeat += ",daily_zoomies_index=" + String(dailyZoomiesIndex, 3);
    heartbeat += ",inactivity_duration_s=" + String(inactivityDurationS(nowMs)) + "i";
    heartbeat += ",inactivity_warning=" + String(inactivityWarningActive(nowMs) ? 1 : 0) + "i";
    heartbeat += ",session_active=" + String(sessionConfirmed ? 1 : 0) + "i";
    heartbeat += ",session_id=" + String(sessionId) + "i";
    heartbeat += ",uptime_ms=" + String(nowMs) + "i";
    heartbeat += ",unix_ts=" + String(static_cast<unsigned long>(unixTs)) + "i";
    appendTimestampIfKnown(heartbeat, unixTs);
    bufferLineIfEnabled(heartbeat);

    if (config::VERBOSE_SERIAL_LOGS)
    {
      Serial.printf("Sample: heartbeat speed=%.1f km/h total=%.2f m ts=%lu\n", speedKmh, totalDistanceM,
                    static_cast<unsigned long>(unixTs));
    }
  }

  DashboardState dashboardState()
  {
    return DashboardState{
        currentSpeedKmh,
        dailyDistanceM,
        dailyRotations,
        sessionConfirmed,
    };
  }
} // namespace wheel_tracker
