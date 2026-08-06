# NautiControl SeaTalk — Direct Wind Fork

This fork adds direct HTTP-based wind data acquisition from a [Garmin-N2K-Mast-Rotation](https://github.com/) ESP32 device, bypassing the need for wind instruments on the SeaTalk bus. Wind angle and speed are polled over WiFi and injected onto the SeaTalk bus for the Raymarine autopilot to consume.

## Changes from Original

| Area | Original (main) | This Fork (direct-wind) |
|------|-----------------|-------------------------|
| Wind source | SeaTalk bus only | HTTP polling from Garmin ESP32 |
| Architecture | Passive bus listener | Active HTTP client + bus writer |
| Code layout | `src/Lib/` subdirectory | Flat `src/` structure |
| WiFi | Not used | Required — connects to espwind AP or local network |
| Web interface | Minimal | Full dashboard with SSE, WebSerial console, OTA updates |
| Logging | Serial only | `log::toAll` — Serial + WebSerial + LittleFS file |
| Configuration | Hardcoded | Web UI for wind server host and button mapping |
| Hardware docs | Gerbers, STL, Fritzing included | Removed (kept in main branch) |

### New Modules

- **WindClient** — HTTP client that polls the Garmin ESP32 for wind data
- **WiFi manager** — Captive portal for network configuration
- **WebSerial console** — Browser-based serial monitor
- **ElegantOTA** — Over-the-air firmware updates
- **Logging system** — Timestamped logs to Serial, WebSerial, and filesystem

## Wind Data Flow

```
┌──────────────────────────────────────────┐
│  Garmin-N2K-Mast-Rotation ESP32          │
│  Serves JSON at /readings:               │
│  { "awa": 45.5, "aws": 12.3, ... }      │
└──────────────────┬───────────────────────┘
                   │  HTTP GET every 1 second
                   ▼
┌──────────────────────────────────────────┐
│  WindClient                              │
│  - Polls http://espwind.local/readings   │
│  - Parses JSON (ArduinoJson)             │
│  - Retries every 10s on failure          │
└──────────────────┬───────────────────────┘
                   │  Calls SeaTalk encode methods
                   ▼
┌──────────────────────────────────────────┐
│  SeaTalk (4800 baud, GPIO 32 RX / 33 TX)│
│  - sendApparentWindAngle(angle)          │
│  - sendApparentWindSpeed(knots)          │
│  - Collision detection with 5x retry    │
│  - Parity-based arbitration              │
└──────────────────┬───────────────────────┘
                   │
                   ▼
         Raymarine Autopilot
         (reads wind from bus)
```

### SeaTalk Message Encoding

- **Wind Angle** `[0x10, 0x01, high, low]` — angle × 2, split into two bytes
- **Wind Speed** `[0x11, 0x01, int & 0x7F, dec & 0x0F]` — integer and fractional knots

### Fallback Behavior

The SeaTalk bus listener still processes incoming messages. If the Garmin ESP32 is unreachable, the system falls back to any wind data already on the bus. The web dashboard uses whichever source has fresher data.

## Web Interface

Served from LittleFS (`/data` directory). Pages:

- **index.html** — Autopilot remote control (button commands)
- **display.html** — Live instrument display (SSE-driven, updates in real time)
- **setup.html** — Configuration (wind server host, button mapping)
- **/wifimanager** — WiFi network setup

Data is pushed to the browser via Server-Sent Events on `/events` with a `new_readings` event containing JSON.

## Build

Requires [PlatformIO](https://platformio.org/). Target board: NodeMCU-32S.

```bash
# Build firmware
pio run -e ec2

# Upload firmware
pio run -e ec2 -t upload

# Upload filesystem (web pages)
pio run -e ec2 -t uploadfs
```

### Key Build Flags

Enabled features (defined in `platformio.ini`):
- `WIFI` — WiFi connectivity
- `WEB` — Async web server
- `SEATALK` — SeaTalk bus read/write
- `WEBSERIAL` — Browser serial console
- `ELEGANTOTA` — Over-the-air updates

## Hardware

- **ESP32** (NodeMCU-32S)
- **GPIO 32** — SeaTalk RX (inverted logic, 4800 baud)
- **GPIO 33** — SeaTalk TX
- **GPIO 2** — Status LED (blinks during bus writes)
- SeaTalk bus requires level shifting to 12V signaling

## Dependencies

- ESPAsyncWebServer / AsyncTCP
- ArduinoJson 7.2.0
- EspSoftwareSerial (parity-aware)
- rc-switch (RF remote control)
- ElegantOTA
- WebSerialPro
- LittleFS
