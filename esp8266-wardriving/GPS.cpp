/* Wardriving GPS module - minimal NMEA-0183 (GPGGA/GPRMC) parser.
 * This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#include "GPS.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

GPS::GPS() {
    line[0] = '\0';
}

void GPS::begin(HardwareSerial& serial, uint32_t baudrate) {
    serial.begin(baudrate);
    _serial = &serial;
}

void GPS::write(uint8_t b) {
    // Ignore everything before the sentence start '$'.
    if (b == '$') {
        lineLen = 0;
        line[lineLen] = '\0';
        return;
    }

    if (b == '\r' || b == '\n') {
        if (lineLen > 0) {
            line[lineLen] = '\0';
            parseLine(line);
            lineLen = 0;
        }
        return;
    }

    if (lineLen < MAX_LINE_LEN - 1) {
        line[lineLen++] = (char)b;
        line[lineLen]   = '\0';
    } else {
        // Overlong/garbage sentence -> discard it.
        lineLen = 0;
    }
}

void GPS::write(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) write(data[i]);
}

bool GPS::update() {
    bool hadFixBefore = pos.valid;

    if (_serial) {
        while (_serial->available() > 0) {
            write((uint8_t)_serial->read());
        }
    }

    return pos.valid && !hadFixBefore;
}

// ---------------------------------------------------------------------------------------------
// Parsing

void GPS::parseLine(char* s) {
    // Strip checksum (*XX).
    char* star = strchr(s, '*');
    if (star) *star = '\0';

    if (!strncmp(s, "GPGGA", 5) || !strncmp(s, "GNGGA", 5)) parseGGA(s);
    else if (!strncmp(s, "GPRMC", 5) || !strncmp(s, "GNRMC", 5)) parseRMC(s);
}

void GPS::parseGGA(char* line) {
    // GGA fields: 1=time 2=lat 3=N/S 4=lon 5=E/W 6=fix 7=sats 8=HDOP 9=alt ...
    char* saveptr = nullptr;
    char* tok     = strtok_r(line, ",", &saveptr); // sentence id (skip)
    const char* fields[10] = { nullptr };
    int         idx        = 0;

    while (tok && idx < 10) {
        tok              = strtok_r(nullptr, ",", &saveptr);
        fields[idx++]    = tok;
    }

    const char* fLat = fields[1];
    const char* fNS  = fields[2];
    const char* fLon = fields[3];
    const char* fEW  = fields[4];
    const char* fFix = fields[5];
    const char* fSat = fields[6];
    const char* fAlt = fields[8];

    int fixQuality = parseInt(fFix);
    if (fixQuality <= 0 || !fLat || !fLon || !*fLat || !*fLon) {
        pos.valid = false;
        return;
    }

    pos.lat   = (float)parseCoord(fLat, fNS && *fNS ? *fNS : 'N');
    pos.lon   = (float)parseCoord(fLon, fEW && *fEW ? *fEW : 'E');
    pos.alt   = parseFloat(fAlt);
    pos.sats  = (uint8_t)parseInt(fSat);
    pos.fix   = (uint8_t)fixQuality;
    pos.valid = true;
}

void GPS::parseRMC(char* line) {
    // RMC fields: 1=time 2=A/V 3=lat 4=N/S 5=lon 6=E/W 7=speed(knots) 8=course ...
    char* saveptr = nullptr;
    char* tok     = strtok_r(line, ",", &saveptr); // sentence id (skip)
    const char* fields[8] = { nullptr };
    int         idx       = 0;

    while (tok && idx < 8) {
        tok           = strtok_r(nullptr, ",", &saveptr);
        fields[idx++] = tok;
    }

    const char* fStat = fields[1];
    const char* fLat  = fields[2];
    const char* fNS   = fields[3];
    const char* fLon  = fields[4];
    const char* fEW   = fields[5];
    const char* fSpd  = fields[6];

    if (!fStat || *fStat != 'A' || !fLat || !fLon || !*fLat || !*fLon) {
        pos.valid = false;
        return;
    }

    pos.lat      = (float)parseCoord(fLat, fNS && *fNS ? *fNS : 'N');
    pos.lon      = (float)parseCoord(fLon, fEW && *fEW ? *fEW : 'E');
    pos.speedKmh = parseFloat(fSpd) * 1.852f; // knots -> km/h
    pos.fix      = pos.fix ? pos.fix : 1;
    pos.valid    = true;
}

double GPS::parseCoord(const char* s, char hemi) {
    // NMEA coordinate: ddmm.mmmm or dddmm.mmmm
    if (!s || !*s) return 0.0;

    double raw = atof(s);

    int    deg       = (int)(raw / 100.0);
    double minutes   = raw - (deg * 100.0);
    double decimal   = deg + (minutes / 60.0);

    switch (hemi) {
        case 'S':
        case 'W':
            decimal = -decimal;
            break;
        default: break;
    }

    return decimal;
}

float GPS::parseFloat(const char* s) {
    if (!s || !*s) return 0.0f;
    return atof(s);
}

int GPS::parseInt(const char* s) {
    if (!s || !*s) return 0;
    return atoi(s);
}
