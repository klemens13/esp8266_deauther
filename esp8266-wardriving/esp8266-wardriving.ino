/* esp8266-wardriving
 * Standalone wardriving sketch for ESP8266: scans Wi-Fi access points, logs them
 * with GPS coordinates from a serial NMEA GPS module to /wardriving.csv on the
 * filesystem, and serves a tiny web UI (map + download) over SoftAP.
 *
 * Wiring (default):
 *   GPS TX  -> ESP8266 D6 (RX)   @ 9600 baud (Serial1 via SoftwareSerial not used; we use Serial swap-free pin mapping below)
 *   Built-in LED on D4 blinks on each new GPS fix.
 *
 * This software is licensed under the MIT License:
 * https://github.com/spacehuhntech/esp8266_deauther */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "GPS.h"
#include "Wardriving.h"

// ===============================================================================================
// Configuration
// ===============================================================================================
#ifndef GPS_BAUDRATE
  #define GPS_BAUDRATE 9600
#endif

// Which hardware serial the GPS is connected to. On most ESP-12F boards:
//   Serial  = UART0 (TX=GPIO1, RX=GPIO3, shared with the USB bridge) - preferred for wardriving
//   Serial1 = UART1 (TX-only on GPIO2; needs a SoftwareSerial/alt pin for RX)
// Default here is UART0 so that normal RX wiring works; change via build flag if needed.
#ifndef GPS_SERIAL_PORT
  #define GPS_SERIAL_PORT Serial
#endif

// If your GPS is on other pins (e.g. D7/D5), comment out the HardwareSerial block below and
// use a SoftwareSerial instead:
//   #include <SoftwareSerial.h>
//   static SoftwareSerial gpsSoftSerial(13, 15); // RX, TX (D7, D5)
// and adapt setup() accordingly.

#ifndef AP_SSID
  #define AP_SSID "wardriving"
#endif
#ifndef AP_PASS
  #define AP_PASS "deauther"
#endif

#define LED_PIN 2 // built-in LED on most ESP-12 modules (active-low)

GPS        gps;
Wardriving wardriving;

ESP8266WebServer server(80);

uint32_t blinkUntil = 0;

// ===============================================================================================
// Web UI
// ===============================================================================================
static String htmlEscape(const String& s) {
    String out = s;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    out.replace("\"", "&quot;");
    return out;
}

static void handleRoot() {
    const GPSPosition& p = gps.getPosition();

    String html = F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>esp8266-wardriving</title>"
        "<style>body{font-family:monospace;margin:1em}pre{background:#111;color:#0f0;padding:1em;"
        "overflow:auto}.btn{display:inline-block;padding:.5em 1em;background:#0a0;color:#fff;"
        "text-decoration:none;margin-right:.5em}</style></head><body>");

    html += "<h2>esp8266-wardriving</h2>";
    html += "<p>GPS fix: ";
    html += p.valid ? "<b style='color:green'>YES" : "<b style='color:red'>NO";
    html += "</b> &nbsp; lat: " + String(p.lat, 6) + " lon: " + String(p.lon, 6);
    html += " alt: " + String(p.alt, 1) + " m &nbsp; sats: " + String(p.sats);
    html += "</p>";
    html += "<p>Logged entries: " + String(wardriving.getLogCount()) + "</p>";
    html += "<p><a class='btn' href='/start'>Start</a>"
            "<a class='btn' href='/stop'>Stop</a>"
            "<a class='btn' href='/scan'>Scan once</a>"
            "<a class='btn' href='/log'>View log</a>"
            "<a class='btn' href='/wardriving.csv'>Download CSV</a></p>";

    html += "<h3>Last 50 lines</h3><pre>";
    File f = LittleFS.open(wardriving.getFilePath(), "r");
    if (f) {
        String all = f.readString();
        f.close();

        // Keep only the last 50 lines to bound memory usage.
        int start = 0;
        int lines = 0;
        for (int i = all.length() - 1; i >= 0; i--) {
            if (all[i] == '\n') {
                lines++;
                if (lines > 50) { start = i + 1; break; }
            }
        }
        html += htmlEscape(all.substring(start));
    } else {
        html += "(no log yet)";
    }
    html += "</pre></body></html>";

    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\nesp8266-wardriving starting..."));

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // LED off (active-low)

    if (!LittleFS.begin()) {
        Serial.println(F("[SETUP] LittleFS mount failed, formatting..."));
        LittleFS.format();
        LittleFS.begin();
    }

    // Default wiring: GPS TX -> GPIO3/D3 (UART0 RX), GPS RX -> GPIO1/D2 (UART0 TX).
    // If you use Serial.swap() (RX=GPIO13, TX=GPIO15), adjust the wiring accordingly.
    gps.begin(GPS_SERIAL_PORT, GPS_BAUDRATE);
    wardriving.setup();

    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print(F("[SETUP] AP IP: "));
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/wardriving.csv", HTTP_GET, []() {
        File f = LittleFS.open(wardriving.getFilePath(), "r");
        if (!f) { server.send(404, "text/plain", "no log yet"); return; }
        server.sendHeader("Content-Disposition", "attachment; filename=wardriving.csv");
        server.streamFile(f, "text/csv");
        f.close();
    });
    server.on("/log", HTTP_GET, []() {
        File f = LittleFS.open(wardriving.getFilePath(), "r");
        if (!f) { server.send(404, "text/plain", "no log yet"); return; }
        server.send(200, "text/plain", f.readString());
        f.close();
    });
    server.on("/start", HTTP_GET, []() { wardriving.start(); server.send(200, "text/plain", "started"); });
    server.on("/stop", HTTP_GET, []() { wardriving.stop(); server.send(200, "text/plain", "stopped"); });
    server.on("/scan", HTTP_GET, []() {
        bool ok = wardriving.logOnce();
        server.send(200, "text/plain", ok ? "scan+log done" : "failed (no GPS fix?)");
    });
    server.begin();

    wardriving.start();
}

void loop() {
    server.handleClient();

    // wardriving.update() already pumps the GPS parser internally; calling gps.update()
    // again would consume bytes before wardriving can log them, so we only use one call.
    bool newFix = wardriving.update();

    if (newFix && gps.hasFix()) {
        blinkUntil = millis() + 80;
        digitalWrite(LED_PIN, LOW); // on
    }
    if (blinkUntil && millis() > blinkUntil) {
        digitalWrite(LED_PIN, HIGH);
        blinkUntil = 0;
    }
}
