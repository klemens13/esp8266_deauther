/* Wardriving GPS module - minimal NMEA-0183 (GPGGA/GPRMC) parser.
 * This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#pragma once

#include <Arduino.h>

struct GPSPosition {
    float   lat      = 0.0f;   // decimal degrees, + = North
    float   lon      = 0.0f;   // decimal degrees, + = East
    float   alt      = 0.0f;   // meters above sea level
    float   speedKmh = 0.0f;   // km/h
    uint8_t sats     = 0;      // satellites used
    uint8_t fix      = 0;      // 0 = no fix, 1 = fix, 2 = DGPS fix
    bool    valid    = false;  // last sentence had a valid fix
};

class GPS {
    public:
        GPS();

        // Begin reading from a hardware serial port (e.g. Serial1).
        void begin(HardwareSerial& serial, uint32_t baudrate);

        // Begin reading from any Stream (e.g. SoftwareSerial) that is already begun.
        void begin(Stream& stream);

        // Call repeatedly from loop(). Returns true when a new valid position was parsed.
        bool update();

        // Feed raw bytes manually (useful for testing or non-serial sources).
        void write(const uint8_t* data, size_t len);
        void write(uint8_t b);

        const GPSPosition& getPosition() const { return pos; }
        bool hasFix() const { return pos.valid; }

    private:
        static constexpr size_t MAX_LINE_LEN = 96; // NMEA sentences are <= 82 chars

        void parseLine(char* line);
        void parseGGA(char* line);
        void parseRMC(char* line);

        static double parseCoord(const char* s, char hemi);
        static float  parseFloat(const char* s);
        static int    parseInt(const char* s);

        HardwareSerial* _serial = nullptr;
        Stream*         _stream = nullptr;
        char   line[MAX_LINE_LEN];
        size_t lineLen = 0;
        GPSPosition pos;
};

extern GPS gps;
