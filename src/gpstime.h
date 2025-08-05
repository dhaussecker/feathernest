#ifndef GPSTIME_H
#define GPSTIME_H

#include "Particle.h"
#include "location.h"

// Structure to hold GPS data
struct GPSData {
    double latitude;
    double longitude;
    time_t timestamp;
    bool valid;
};

// Function declarations
GPSData getGPSData();
void timerCallback();
void initializeGPS();
void checkGPSUpdate();

// External timer declaration - DISABLED
// extern Timer gpsTimer;
extern unsigned long lastGPSUpdateTime;
extern const unsigned long GPS_UPDATE_INTERVAL;

#endif // GPSTIME_H