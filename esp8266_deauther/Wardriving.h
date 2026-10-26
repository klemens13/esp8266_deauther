/* Wardriving logger - periodically scans Wi-Fi access points and appends them
 * together with the current GPS position to a CSV file on the SPIFFS filesystem.
 * The resulting /wardriving.csv can be converted to KML/GPX for mapping tools
 * (e.g. with https://github.com/geohot/wardriver scripts or GPSBabel).
 *
 * This software is licensed under the MIT License:
 * https://github.com/spacehuhntech/esp8266_deauther */

#pragma once

#include <Arduino.h>
#include "GPS.h"

#define WARDRIVING_FILE      "/wardriving.csv"
#define WARDRIVING_SSID      "wardriving"
#define WARDRIVING_PASS      "deauther"
#define WARDRIVING_INTERVAL  10000UL // ms between scan+log cycles

class Wardriving {
    public:
        Wardriving();

        void setup();
        void update();

        void start();
        void stop();

        bool isRunning() const { return running; }
        uint32_t getLogCount() const { return logCount; }

        // Force one scan+log cycle right now (used by CLI/web command).
        bool logOnce();

        String getFilePath() const { return filePath; }

    private:
        void ensureWifiMode();
        bool writeRow(const String& bssid, const String& ssid, int rssi, uint8_t ch);
        bool appendToCSV(String& buf);

        bool     initialized = false;
        bool     running     = false;
        uint32_t lastRun     = 0;
        uint32_t logCount    = 0;
        String   filePath    = WARDRIVING_FILE;
};

extern Wardriving wardriving;
