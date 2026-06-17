#include "WifiTime.h"

#include <WiFi.h>

#include "Config.h"

namespace
{
  uint32_t lastWifiAttemptMs = 0;
  bool timeInitialized = false;
} // namespace

namespace wifi_time
{
  void connectWifiIfNeeded()
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      return;
    }

    const uint32_t nowMs = millis();
    if (nowMs - lastWifiAttemptMs < config::WIFI_RETRY_INTERVAL_MS)
    {
      return;
    }
    lastWifiAttemptMs = nowMs;

    Serial.printf("WiFi: Verbinde mit SSID '%s'...\n", config::WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(config::WIFI_SSID, config::WIFI_PASSWORD);
  }

  void initTimeIfNeeded()
  {
    if (timeInitialized || WiFi.status() != WL_CONNECTED)
    {
      return;
    }

    configTzTime(config::TIMEZONE, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    uint8_t tries = 0;
    while (now < 1700000000 && tries < 20)
    {
      delay(200);
      now = time(nullptr);
      tries++;
    }

    timeInitialized = (now >= 1700000000);
    Serial.printf("Zeit-Sync: %s\n", timeInitialized ? "OK" : "fehlgeschlagen, retry spaeter");
  }

  bool initialized()
  {
    return timeInitialized;
  }

  int32_t localDayKey(time_t unixTs)
  {
    if (unixTs < 1700000000)
    {
      return -1;
    }

    struct tm timeInfo;
    localtime_r(&unixTs, &timeInfo);
    return (timeInfo.tm_year + 1900) * 1000 + timeInfo.tm_yday;
  }
} // namespace wifi_time
