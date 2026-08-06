# N2K CAN Bus Integration — STW-wjquigs

## Summary

Added NMEA 2000 (N2K) CAN bus support as an **optional alternate wind data source**.
Previously, wind data was obtained exclusively via HTTP polling the Garmin-N2K-Mast-Rotation
ESP32 (`WindClient`). Now the device can also read wind directly from an N2K bus.

## Architecture

- **Listen-only**: The ESP32 native CAN controller reads N2K frames but never transmits.
- **Wind PGN 130306 only**: Only apparent wind data is parsed. All other PGNs are ignored.
- **Feeds SignalManager**: Received AWA/AWS values go through the same `SignalManager` path
  as data from the TCP WindClient, ultimately reaching `SeaTalkData` and the SeaTalk bus.
- **Coexists with WindClient**: Both sources can be active simultaneously. Whichever
  delivers data most recently wins (last-write-wins to SeaTalkData fields).

## Hardware

Default CAN pins (defined in `n2k.cpp`, adjust for your board):
- `N2K_CAN_TX_PIN` = GPIO 5
- `N2K_CAN_RX_PIN` = GPIO 4

A CAN transceiver (e.g., SN65HVD230 or MCP2551) is required between the ESP32 and the
NMEA 2000 bus.

## Files Modified

| File | Change |
|------|--------|
| `src/n2k.cpp` | Complete rewrite — simplified listen-only wind PGN reader |
| `src/include.h` | Added `#ifdef N2K` section with externs and function declarations |
| `src/main.cpp` | Added `n2kSetup()` in setup, `n2kLoop()` in loop |
| `src/webserial.cpp` | Added `n2k` command and `n2kdebug` toggle |
| `platformio.ini` | Added NMEA2000 library dependencies |

## Library Dependencies Added

```
ttlappalainen/NMEA2000-library@4.21.5
https://github.com/wjquigs11/NMEA2000_esp32
```

## Build Flag

N2K support is enabled by `-D N2K` in platformio.ini build_flags (already present).
Remove or comment it to compile without CAN support.

## WebSerial Commands

| Command | Description |
|---------|-------------|
| `n2k` | Print N2K status (open, active, message counts, last wind values) |
| `toggle n2kdebug` | Toggle verbose N2K PGN logging |
| `windtcp` | Toggle the TCP WindClient on/off at runtime |
| `wind` | Print WindClient status (host, connected, success/fail counts) |

## WindClient TCP Toggle

The TCP connection to the Garmin-N2K-Mast-Rotation ESP32 (`WindClient`) now **defaults to
disabled** on boot. This avoids unnecessary network traffic when N2K CAN is the primary
wind source.

Ways to enable/disable at runtime:
- **Web UI**: `setup.html` has a "TCP Wind Enabled" checkbox under the Wind Server section.
  Toggling it sends `GET /windtcp?enabled=1` or `=0` to the ESP.
- **WebSerial**: Type `windtcp` to flip the state.
- **HTTP API**: `GET /windtcp?enabled=1` or `GET /windtcp?enabled=0`. Without a param it
  returns the current state.

When disabled, the HTTP connection is closed and no polling occurs. Re-enabling resumes
polling at the configured interval.

### Additional files modified for WindClient toggle

| File | Change |
|------|--------|
| `src/WindClient.h` | Added `enabled` field (default `false`) and `setEnabled()` method |
| `src/WindClient.cpp` | Early-return in `loop()` when `!enabled` |
| `src/webserver.cpp` | Added `/windtcp` endpoint; `windtcp` field in `/GetOptions.json` |
| `src/webserial.cpp` | Added `windtcp` command and `n2kdebug` toggle |
| `data/setup.html` | Added TCP Wind Enabled checkbox |
| `data/nauticontrol.js` | Added `toggleWindTcp()`, loads state from `GetOptions.json` |

## Future Work

- Add STW PGN 128259 parsing for speed-through-water as alternate STW source
- Add SOG/COG PGN 129026 parsing
- Priority logic: prefer N2K over WindClient when both are delivering data
- Configurable CAN pins via preferences or settings.json
