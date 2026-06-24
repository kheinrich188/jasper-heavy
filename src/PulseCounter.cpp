#include "PulseCounter.h"

#include "Config.h"

namespace
{
  volatile uint32_t pulseCount = 0;
  volatile uint32_t lastPulseUs = 0;
  portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;

  void IRAM_ATTR onHallPulse()
  {
    const uint32_t nowUs = micros();

    portENTER_CRITICAL_ISR(&pulseMux);
    if (nowUs - lastPulseUs >= config::PULSE_DEBOUNCE_US)
    {
      pulseCount = pulseCount + 1;
      lastPulseUs = nowUs;
    }
    portEXIT_CRITICAL_ISR(&pulseMux);
  }
} // namespace

namespace pulse_counter
{
  void begin(int pin)
  {
    pinMode(pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pin), onHallPulse, FALLING);
  }

  uint32_t take()
  {
    portENTER_CRITICAL(&pulseMux);
    const uint32_t pulses = pulseCount;
    pulseCount = 0;
    portEXIT_CRITICAL(&pulseMux);
    return pulses;
  }

  uint32_t pending()
  {
    portENTER_CRITICAL(&pulseMux);
    const uint32_t pulses = pulseCount;
    portEXIT_CRITICAL(&pulseMux);
    return pulses;
  }
} // namespace pulse_counter
