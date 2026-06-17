#pragma once

#include <Arduino.h>

#ifndef CAT_WHEEL_WIFI_SSID
#define CAT_WHEEL_WIFI_SSID ""
#endif

#ifndef CAT_WHEEL_WIFI_PASSWORD
#define CAT_WHEEL_WIFI_PASSWORD ""
#endif

#ifndef CAT_WHEEL_INFLUX_BASE_URL
#define CAT_WHEEL_INFLUX_BASE_URL ""
#endif

#ifndef CAT_WHEEL_INFLUX_ORG
#define CAT_WHEEL_INFLUX_ORG ""
#endif

#ifndef CAT_WHEEL_INFLUX_BUCKET
#define CAT_WHEEL_INFLUX_BUCKET ""
#endif

#ifndef CAT_WHEEL_INFLUX_TOKEN
#define CAT_WHEEL_INFLUX_TOKEN ""
#endif

namespace config
{
  constexpr const char *WIFI_SSID = CAT_WHEEL_WIFI_SSID;
  constexpr const char *WIFI_PASSWORD = CAT_WHEEL_WIFI_PASSWORD;

  // Heltec V3: Bitte im V3.2 Pinmap pruefen, ob die Header-Labels 1/2/3 auf diese GPIOs zeigen.
  constexpr int HALL_SENSOR_PIN = 1;
  constexpr int ALT_SENSOR_PIN_2 = 2;
  constexpr int ALT_SENSOR_PIN_3 = 3;

  constexpr float PI_F = 3.14159265f;
  constexpr float INNER_DIAMETER_M = 1.252f;                       // 125.2 cm
  constexpr float OUTER_DIAMETER_M = 1.280f;                       // 128.0 cm (Magnet-Position)
  constexpr float INNER_CIRCUMFERENCE_M = PI_F * INNER_DIAMETER_M; // Katzen-Laufstrecke pro Umdrehung
  constexpr float OUTER_CIRCUMFERENCE_M = PI_F * OUTER_DIAMETER_M;
  constexpr int MAGNETS_PER_ROTATION = 4;

  constexpr const char *INFLUX_BASE_URL = CAT_WHEEL_INFLUX_BASE_URL;
  constexpr const char *INFLUX_ORG = CAT_WHEEL_INFLUX_ORG;
  constexpr const char *INFLUX_BUCKET = CAT_WHEEL_INFLUX_BUCKET;
  constexpr const char *INFLUX_TOKEN = CAT_WHEEL_INFLUX_TOKEN;

  constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;
  constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
  constexpr uint32_t SYNC_INTERVAL_MS = 15000;
  constexpr uint32_t DISPLAY_INTERVAL_MS = 1000;
  constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 4000;
  constexpr uint32_t HTTP_RESPONSE_TIMEOUT_MS = 6000;
  constexpr uint32_t HEARTBEAT_INTERVAL_MS = 600000;
  constexpr uint32_t SESSION_IDLE_TIMEOUT_MS = 20000;
  constexpr uint32_t PULSE_DEBOUNCE_US = 5000;
  constexpr size_t MAX_BUFFER_BYTES = 32768;
  constexpr const char *BUFFER_FILE = "/catwheel.lp";
  constexpr const char *SYNC_BUFFER_FILE = "/catwheel-sync.lp";
  constexpr bool PURGE_BUFFER_ON_BOOT = false;

  constexpr const char *TIMEZONE = "CET-1CEST,M3.5.0,M10.5.0/3";

  constexpr uint8_t OLED_WIDTH = 128;
  constexpr uint8_t OLED_HEIGHT = 64;
  constexpr int OLED_SDA_PIN = 17;
  constexpr int OLED_SCL_PIN = 18;
  constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;
  constexpr int OLED_RESET_PIN = 21;
  constexpr int OLED_VEXT_PIN = 36; // Heltec Vext is active LOW
} // namespace config
