/* Wardriving logger implementation.
 * This software is licensed under the MIT License:
 * https://github.com/spacehuhntech/esp8266_deauther */

#include "Wardriving.h"

#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <math.h>

// Use the same filesystem object as the deauther firmware (functions.h uses LittleFS directly).
#ifndef WARDRIVING_FS
  #define WARDRIVING_FS LittleFS
#endif

// -----------------------------------------------------------------------------------------------
// Helpers

static String macToStr(const uint8_t* m) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(buf);
}

static String sanitizeSSID(String s) {
    // Replace characters that would break the CSV and strip control chars.
    s.replace("\"", "\"\"");   // CSV escaping
    s.replace('\r', ' ');
    s.replace('\n', ' ');
    for (size_t i = 0; i < s.length(); i++) {
        if ((uint8_t)s[i] < 32) s.setCharAt(i, ' ');
    }
    if (s.length() > 64) s = s.substring(0, 64);
    return "\"" + s + "\"";
}

// -----------------------------------------------------------------------------------------------

Wardriving::Wardriving() {}

void Wardriving::setup() {
    initialized = true;
}

void Wardriving::ensureWifiMode() {
    if (WiFi.getMode() != WIFI_MODE_STA) {
        WiFi.mode(WIFI_MODE_STA);
        delay(10);
    }
}

void Wardriving::start() {
    running  = true;
    lastRun  = 0; // run first cycle immediately on next update()
    Serial.println(F("[WARDRIVING] started logging to " WARDRIVING_FILE));
}

void Wardriving::stop() {
    running = false;
    Serial.println(F("[WARDRIVING] stopped"));
}

void Wardriving::update() {
    gps.update();

    if (!running || !initialized) return;

    if (millis() - lastRun >= WARDRIVING_INTERVAL) {
        lastRun = millis();
        logOnce();
    }
}

bool Wardriving::logOnce() {
    ensureWifiMode();

    if (!gps.hasFix()) {
        Serial.println(F("[WARDRIVING] no GPS fix yet - waiting"));
        return false;
    }

    // Non-blocking-ish scan of all channels.
    int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    if (n <= 0) {
        Serial.println(F("[WARDRIVING] scan found no networks"));
        WiFi.scanDelete();
        return false;
    }

    const GPSPosition& p = gps.getPosition();

    bool ok = true;
    for (int i = 0; i < n; i++) {
        uint8_t* bssid = WiFi.BSSID(i);
        if (!bssid) continue;
        ok &= writeRow(macToStr(bssid), WiFi.SSID(i), WiFi.RSSI(i), (uint8_t)WiFi.channel(i));
    }

    WiFi.scanDelete();
    logCount += n;
    Serial.printf("[WARDRIVING] logged %d networks @ %.6f,%.6f\n", n, p.lat, p.lon);
    return ok;
}

bool Wardriving::writeRow(const String& bssid, const String& ssid, int rssi, uint8_t ch) {
    const GPSPosition& p = gps.getPosition();

    char latBuf[16], lonBuf[16], altBuf[12];
    dtostrf(p.lat, 9, 6, latBuf);
    dtostrf(p.lon, 10, 6, lonBuf);
    dtostrf(p.alt, 7, 1, altBuf);

    String line;
    line.reserve(128);
    line += String(millis() / 1000UL);        // uptime seconds
    line += ",";
    line += String(latBuf);                    // latitude
    line += ",";
    line += String(lonBuf);                    // longitude
    line += ",";
    line += String(altBuf);                    // altitude (m)
    line += ",";
    line += String(rssi);                      // signal dBm
    line += ",";
    line += String(ch);                        // channel
    line += ",";
    line += bssid;                             // BSSID
    line += ",";
    line += sanitizeSSID(ssid);                // SSID (quoted/escaped)
    line += "\n";

    return appendToCSV(line);
}

bool Wardriving::appendToCSV(String& buf) {
    if (!WARDRIVING_FS.exists(filePath)) {
        // Write header once.
        File f = WARDRIVING_FS.open(filePath, "w");
        if (!f) {
            Serial.println(F("[WARDRIVING] ERROR: could not create log file"));
            return false;
        }
        f.println("time_s,lat,lon,alt,rssi,ch,bssid,ssid");
        f.close();
    }

    File f = WARDRIVING_FS.open(filePath, "a");
    if (!f) {
        Serial.println(F("[WARDRIVING] ERROR: could not open log file for appending"));
        return false;
    }
    size_t written = f.print(buf);
    f.close();

    return written == buf.length();
}
