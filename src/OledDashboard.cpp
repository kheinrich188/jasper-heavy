#include "OledDashboard.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "Config.h"

namespace
{
  Adafruit_SSD1306 display(config::OLED_WIDTH, config::OLED_HEIGHT, &Wire, config::OLED_RESET_PIN);
  bool displayReady = false;

  struct BusCandidate
  {
    int sda;
    int scl;
  };

  void beginBus(int sda, int scl)
  {
    Wire.end();
    if (sda >= 0 && scl >= 0)
    {
      Wire.begin(sda, scl);
    }
    else
    {
      Wire.begin();
    }
    Wire.setClock(100000);
    delay(10);
  }

  bool probeI2CAddress(uint8_t address)
  {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
  }

  void logScanResult(const BusCandidate &bus)
  {
    String found;
    for (uint8_t address = 1; address < 127; address++)
    {
      Wire.beginTransmission(address);
      if (Wire.endTransmission() == 0)
      {
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%02X ", address);
        found += buf;
      }
    }
    if (found.length() == 0)
    {
      Serial.printf("OLED: Kein I2C Device auf Bus (SDA=%d, SCL=%d)\n", bus.sda, bus.scl);
    }
    else
    {
      Serial.printf("OLED: I2C Devices auf Bus (SDA=%d, SCL=%d): %s\n", bus.sda, bus.scl, found.c_str());
    }
  }
} // namespace

namespace oled_dashboard
{
  void begin()
  {
    pinMode(config::OLED_VEXT_PIN, OUTPUT);
    digitalWrite(config::OLED_VEXT_PIN, LOW);
    pinMode(config::OLED_RESET_PIN, OUTPUT);
    digitalWrite(config::OLED_RESET_PIN, HIGH);
    delay(50);

    const BusCandidate buses[] = {
        {config::OLED_SDA_PIN, config::OLED_SCL_PIN},
        {17, 18},
        {18, 17},
        {-1, -1},
    };
    const uint8_t addresses[] = {
        config::OLED_I2C_ADDRESS,
        0x3C,
        0x3D,
    };

    for (const BusCandidate &bus : buses)
    {
      beginBus(bus.sda, bus.scl);
      logScanResult(bus);
      for (uint8_t address : addresses)
      {
        if (!probeI2CAddress(address))
        {
          continue;
        }
        if (display.begin(SSD1306_SWITCHCAPVCC, address))
        {
          displayReady = true;
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(0, 0);
          display.println("Cat Wheel Tracker");
          display.println("Display bereit");
          display.display();
          Serial.printf("OLED: Verbunden auf I2C 0x%02X (SDA=%d, SCL=%d)\n", address, bus.sda, bus.scl);
          return;
        }
      }
    }

    Serial.println("OLED: Kein I2C-Display gefunden (0x3C/0x3D).");
    displayReady = false;
  }

  void update(float currentSpeedKmh, float dailyDistanceM, float dailyRotations, bool online)
  {
    if (!displayReady)
    {
      return;
    }

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    const int roundedRotations = static_cast<int>(floorf(dailyRotations));
    const String rotationValue = String(roundedRotations);
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    display.setTextSize(1);
    display.getTextBounds("Runden", 0, 0, &x1, &y1, &w, &h);
    const int rightLabelX = static_cast<int>(display.width()) - static_cast<int>(w) - 2;
    display.setCursor(rightLabelX, 0);
    display.println("Runden");
    display.setTextSize(3);
    display.getTextBounds(rotationValue, 0, 0, &x1, &y1, &w, &h);
    const int rightNumberX = static_cast<int>(display.width()) - static_cast<int>(w) - 2;
    display.setCursor(rightNumberX, 16);
    display.print(rotationValue);

    display.setTextSize(2);
    display.setCursor(0, 8);
    display.print(currentSpeedKmh, 1);
    display.setTextSize(1);
    display.setCursor(0, 26);
    display.println("km/h");

    display.setTextSize(2);
    display.setCursor(0, 40);
    display.print(dailyDistanceM, 1);
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.println("m");

    const char *status = online ? "ON" : "OFF";
    display.setTextSize(1);
    display.getTextBounds(status, 0, 0, &x1, &y1, &w, &h);
    const int statusX = static_cast<int>(display.width()) - static_cast<int>(w) - 2;
    display.setCursor(statusX, 56);
    display.print(status);

    display.display();
  }
} // namespace oled_dashboard
