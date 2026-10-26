# esp8266-wardriving

Standalone wardriving sketch for ESP8266, companion to the [esp8266_deauther](../esp8266_deauther) firmware.

It periodically scans nearby Wi-Fi access points and appends each one — together with the
current position from a serial NMEA GPS module — to `/wardriving.csv` on the flash filesystem.
A tiny web UI (served over a SoftAP) shows live GPS status, lets you start/stop logging, run a
one-shot scan, view the log, and download the CSV for mapping (KML/GPX conversion via GPSBabel,
MyEsimap, apsnmap, etc.).

## Files

| File | Description |
|------|-------------|
| `esp8266-wardriving.ino` | Main sketch: setup, web server, LED feedback |
| `GPS.h` / `GPS.cpp` | Minimal NMEA-0183 parser (`GPGGA`/`GPRMC`, also `GN*` constellations) |
| `Wardriving.h` / `Wardriving.cpp` | Scan + CSV logger (LittleFS/SPIFFS compatible) |

## Wiring (defaults)

| Signal | ESP8266 pin |
|--------|-------------|
| GPS TX → UART0 RX | GPIO3 / D3 |
| GPS RX → UART0 TX | GPIO1 / D2 (optional) |
| Status LED | GPIO2 / D4 (built-in LED, active-low) |

The GPS is read from `Serial` (UART0) at 9600 baud by default. Override at compile time with
`-DGPS_SERIAL_PORT=Serial1 -DGPS_BAUDRATE=115200` or edit the defines at the top of the `.ino`.
If your GPS sits on other pins, use `SoftwareSerial` (see comment in the sketch) or
`Serial.swap()` (RX=GPIO13/D7, TX=GPIO15/D8).

## Access point / web UI

- SSID: `wardriving` / password: `deauther` (override with `-DAP_SSID=... -DAP_PASS=...`)
- URL: `http://192.168.4.1/`

Endpoints: `/` (status), `/start`, `/stop`, `/scan` (log once now), `/log` (plain text),
`/wardriving.csv` (download).

## CSV format

```
time_s,lat,lon,alt,rssi,ch,bssid,ssid
1234,48.135125,11.581943,520.3,-67,6,AA:BB:CC:DD:EE:FF,"ExampleNet"
```

## Logging integration with esp8266_deauther

The `GPS` and `Wardriving` classes are self-contained; to add wardriving to the main deauther
firmware, copy `GPS.*` and `Wardriving.*` into `esp8266_deauther/`, instantiate them in the
`.ino`, call `wardriving.update()` from `loop()`, and hook `logOnce()` into the CLI/web
commands. The deauther's own sniffer data (`accesspoints.getMacStr(i)`, `getRSSI(i)`,
`getCh(i)`) can replace the `WiFi.scanNetworks()` pass for denser logs.

## Building

Arduino IDE: select board "Generic ESP8266 Module" (or your NodeMCU/Wemos variant), open
`esp8266-wardriving.ino`, upload. Requires LittleFS (or SPIFFS) filesystem support and the
`ESP8266WebServer` library shipped with the ESP8266 core.
