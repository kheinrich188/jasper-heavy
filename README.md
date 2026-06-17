# Jasper Heavy - Cat Wheel Tracker

Firmware for a **Heltec WiFi LoRa 32 V3 (ESP32-S3)** that tracks cat wheel activity with a Hall sensor, shows live KPIs on the OLED, buffers data offline, and syncs to InfluxDB.

## Features

- Hall sensor pulse counting with interrupt
  - ISR-side debounce via `PULSE_DEBOUNCE_US`
- 4 magnets per full wheel rotation
- Calculated metrics:
  - speed (km/h)
  - rpm
  - daily distance (m), reset by configured local timezone
  - daily rotations (integer)
  - total distance and total rotations
- Session tracking:
  - session start/end
  - session duration
  - session distance
  - session rotations
  - session max speed
  - session id + active flag
- OLED dashboard:
  - left: current speed + daily meters
  - right: large daily rotations
- Offline-first sync:
  - local SPIFFS buffering
  - sync snapshot file so uploads do not erase newly collected samples
  - periodic background upload task
  - original sample timestamps are written to Influx line protocol when NTP time is available
  - payload sanitation for robust retry behavior
- Influx is optional:
  - if Influx env vars are not set, firmware runs in display-only mode
  - wheel tracking and OLED continue to work without cloud upload

## Hardware

- Heltec WiFi LoRa 32 V3
- Hall sensor (A3144 / 3144E type)
- 4 magnets on the wheel
- Cat wheel: elmato Katzenlaufrad, size 140  
  https://www.elmato.de/collections/katzenlaufrad

Configured constants:

- `MAGNETS_PER_ROTATION = 4`
- `PULSE_DEBOUNCE_US = 5000`
- `INNER_DIAMETER_M = 1.252` (125.2 cm)
- `OUTER_DIAMETER_M = 1.280` (128.0 cm)
- direction is fixed as clockwise (`direction=clockwise` tag in telemetry)
- timezone is `CET-1CEST,M3.5.0,M10.5.0/3` for local daily counters

## Project Setup

### 1. Configure secrets

Copy `.env.example` to `.env` and fill values:

```env
WIFI_SSID=dein-wlan-ssid
WIFI_PASSWORD=dein-wlan-passwort

INFLUX_BASE_URL=https://your-influx-host
INFLUX_ORG=your-org
INFLUX_BUCKET=cat-wheel
INFLUX_TOKEN=your-token
```

The `.env` is loaded by `scripts/load_env.py` via PlatformIO and is ignored by git.

If `INFLUX_BASE_URL`, `INFLUX_ORG`, `INFLUX_BUCKET`, or `INFLUX_TOKEN` are missing, upload is automatically disabled.

### 2. Build / Flash

Use PlatformIO (VS Code):

```bash
pio run
pio run -t upload
pio device monitor
```

If `pio` is not in PATH, use PlatformIO UI in VS Code.

## Code Layout

The firmware is split into small modules:

- `src/main.cpp` - setup/loop orchestration, sample/display/sync scheduling
- `include/Config.h` - hardware pins, intervals, geometry, Influx and OLED constants
- `src/PulseCounter.cpp` - Hall sensor interrupt, debounce, atomic pulse draining
- `src/WheelTracker.cpp` - rotations, speed, distance, daily counters, session tracking, telemetry line creation
- `src/OledDashboard.cpp` - OLED power/init/probing and dashboard rendering
- `src/BufferStore.cpp` - SPIFFS append buffer, sync snapshot, oversized-buffer handling
- `src/InfluxClient.cpp` - Influx URL creation, payload sanitation, HTTP upload
- `src/WifiTime.cpp` - WiFi reconnect and NTP/local timezone setup

## Telemetry Model (Influx Line Protocol)

Measurement: `cat_wheel`  
Tags:
- `device=heltec_v3`
- `direction=clockwise`

Main fields:
- `pulses`, `rotations`, `rpm`
- `speed_kmh`
- `distance_total_m`, `rotations_total`
- `daily_distance_m`, `daily_rotations`
- `session_id`, `session_active`
- `session_duration_s`, `session_distance_m`, `session_rotations`, `session_max_kmh`
- `session_ended` (on end event)
- `heartbeat` (idle heartbeat event)
- `uptime_ms`, `unix_ts`

When NTP time is available, the Unix timestamp is also appended as the actual Influx point timestamp. This keeps offline-buffered data on the real event time instead of the later upload time.

## Session Logic

- Session starts when pulses are detected after idle.
- Session ends after `SESSION_IDLE_TIMEOUT_MS` without pulses.
- Session state is included in moving and heartbeat points.

## Buffering / Sync Behavior

- New telemetry lines are appended to `/catwheel.lp`.
- A sync attempt renames the active buffer to `/catwheel-sync.lp`.
- While the sync snapshot uploads, new samples continue to collect in a fresh active buffer.
- On successful upload, only the sync snapshot is removed.
- On failed upload, the snapshot remains and is retried later.
- Boot-time buffer purging is disabled by default with `PURGE_BUFFER_ON_BOOT = false`.

## Dashboard Recommendations

Start with InfluxDB UI or Grafana:

- Daily distance and daily rotations
- Current and max speed
- Session duration/distance/max speed
- Activity by hour of day
- 7-day trend and best day

## Notes

- OLED + I2C initialization includes Heltec Vext power handling and bus/address probing.
- Upload runs in a separate FreeRTOS task so the main loop keeps sampling/displaying even if HTTP stalls.
- Total distance, total rotations, and session id are kept in RAM and reset after reboot.
