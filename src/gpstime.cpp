#include "gpstime.h"

// Global timer instance - DISABLED due to BLE conflicts
// Timer gpsTimer(300000, timerCallback);

// Use manual timing instead
unsigned long lastGPSUpdateTime = 0;
const unsigned long GPS_UPDATE_INTERVAL = 120000; // 2 minutes

// Function that returns latitude, longitude, and unix timestamp
GPSData getGPSData()
{
    GPSData data = {0.0, 0.0, 0, false};
    
    LocationPoint point = {};
    // Use non-blocking GPS call to prevent system hangs
    auto results = Location.getLocation(point, false);
    
    if (results == LocationResults::Fixed) {
        data.latitude = point.latitude;
        data.longitude = point.longitude;
        data.timestamp = Time.now();
        data.valid = true;
    }
    
    return data;
}

// Timer callback function
void timerCallback()
{
    Log.info("=== GPS TIMER FIRED - Starting GPS location update ===");
    
    // Check if BLE is scanning or connected
    extern bool isScanning;
    extern bool isConnected;
    bool wasScanning = false;
    
    // Skip GPS if actively connected to a BLE device
    if (isConnected) {
        Log.warn("Skipping GPS update - BLE device connected, avoiding radio conflict");
        return;
    }
    
    if (isScanning) {
        Log.info("Pausing BLE scan for GPS update");
        BLE.stopScanning();
        wasScanning = true;
        isScanning = false;
        // Give radio time to switch
        delay(500);
    }
    
    // WARNING: This may block for up to 90 seconds if GPS doesn't have a fix
    Log.warn("Attempting GPS read - may block for up to 90 seconds!");
    
    GPSData gpsData = getGPSData();
    
    if (gpsData.valid) {
        Log.info("GPS Data - Lat: %.6f, Lon: %.6f, Timestamp: %lld", 
                 gpsData.latitude, gpsData.longitude, (long long)gpsData.timestamp);
        
        // Publish to cloud with actual GPS coordinates and time
        char publishData[128];
        snprintf(publishData, sizeof(publishData), 
            "{\"lat\":%.6f,\"lon\":%.6f,\"timestamp\":%lld}",
            gpsData.latitude, gpsData.longitude, (long long)gpsData.timestamp);
        
        Particle.publish("location-update", publishData, PRIVATE);
        Log.info("GPS location published successfully");
    } else {
        Log.info("GPS fix not available");
        
        // Still publish with timestamp even without GPS fix
        char publishData[128];
        snprintf(publishData, sizeof(publishData), 
            "{\"lat\":0.0,\"lon\":0.0,\"timestamp\":%lld,\"status\":\"no_fix\"}",
            (long long)Time.now());
        
        Particle.publish("location-update", publishData, PRIVATE);
    }
    
    // Resume BLE scanning if it was active before
    if (wasScanning) {
        delay(500); // Give radio time to switch back
        Log.info("Resuming BLE scan after GPS update");
        // BLE scan will resume in next loop iteration
    }
    
    Log.info("=== GPS TIMER CALLBACK COMPLETE ===");
}

// Initialize GPS functionality
void initializeGPS()
{
    LocationConfiguration config;
    // Identify and enable active GNSS antenna power
    config.enableAntennaPower(GNSS_ANT_PWR);
    // Assign buffer to encoder.
    Location.begin(config);
    
    // Initialize timing
    lastGPSUpdateTime = millis();
    
    // Timer disabled due to BLE conflicts
    // gpsTimer.start();
}

// Check if GPS update is needed (call from main loop)
void checkGPSUpdate()
{
    if (millis() - lastGPSUpdateTime >= GPS_UPDATE_INTERVAL) {
        lastGPSUpdateTime = millis();
        timerCallback();
    }
}