
#include "Particle.h"
#include "Arduino.h"
#include "ble.h"
#include "gpstime.h"

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

// Show system, cloud connectivity, and application logs over USB
// View logs with CLI using 'particle serial monitor --follow'
SerialLogHandler logHandler(LOG_LEVEL_INFO);

void setup() 
{
    Serial.begin(9600);
    while(!Serial){}

    Log.info("V8 Central v3 Starting...");
    
    // Initialize BLE
    if (!initBLE()) {
        Log.error("Failed to initialize BLE!");
        // Continue anyway, maybe it will work later
    }

    // Initialize GPS first
    initializeGPS();
    Log.info("Location timer started - will update every 2 minutes");
    
    // Wait for cloud connection to stabilize before starting BLE
    waitFor(Particle.connected, 30000);
    if (!Particle.connected()) {
        Log.warn("Cloud connection not established after 30 seconds");
    }
    
    // Give system time to stabilize after cloud connection
    delay(3000);
    
    // Start scanning for devices
    if (startScanning()) {
        Log.info("Scanning for BLE devices...");
    } else {
        Log.error("Failed to start scanning!");
    }
}

// loop() runs over and over again, as quickly as it can execute.
void loop() 
{   
    // Feed the application watchdog to prevent system resets
    Particle.process();
    
    // Handle pending disconnect request FIRST (avoid BLE callback disconnect bug)
    if (disconnectRequested && isConnected) {
        disconnectRequested = false;
        
        Log.info("Processing disconnect request from main loop");
        
        // Send ACK before disconnecting (from main loop, not callback)
        Log.info("Attempting to send ACK with timeout protection...");
        Log.info("Sending ACK to peripheral...");
        
        if (sendAckWithTimestamp()) {
            Log.info("ACK sent successfully from main loop");
        } else {
            Log.warn("ACK send failed from main loop");
        }
        
        // Now disconnect regardless of ACK result
        Log.info("Disconnecting to scan for next device...");
        forceDisconnect();
        
        // Move to next target device
        currentTargetIndex = (currentTargetIndex + 1) % NUM_TARGET_DEVICES;
        Log.info("Moving to next target device: %s", TARGET_DEVICE_NAMES[currentTargetIndex]);
    }
    
    // GPS timing now handled in gpstime module
    
    // Scan for devices periodically (every 15 seconds) only when not connected
    static unsigned long lastScanTime = 0;
    static unsigned long scanStartTime = 0;
    static bool scanInProgress = false;
    
    if (!isConnected && (millis() - lastScanTime >= 15000)) {
        lastScanTime = millis();
        scanStartTime = millis();
        scanInProgress = true;
        
        Log.info("Starting periodic BLE scan...");
        startScanning();
        scanInProgress = false;
        Log.info("Scan completed or timed out");
        Log.info("Test3.7");
    }
    
    // Watchdog: If scan has been running too long, something is wrong
    if (scanInProgress && (millis() - scanStartTime > 30000)) {
        Log.warn("BLE scan appears to be hanging - will retry in next cycle");
        scanInProgress = false;
        lastScanTime = millis() - 10000; // Retry sooner
    }
    
    // Show connection status and GPS timer countdown periodically
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime >= 10000) {
        lastStatusTime = millis();
        
        // Calculate GPS timer countdown using the new manual timing
        unsigned long elapsedTime = millis() - lastGPSUpdateTime;
        unsigned long remainingTime = 0;
        
        if (elapsedTime < GPS_UPDATE_INTERVAL) {
            remainingTime = GPS_UPDATE_INTERVAL - elapsedTime;
        } else {
            remainingTime = 0; // Update is due
        }
        
        int secondsRemaining = remainingTime / 1000;
        int minutesRemaining = secondsRemaining / 60;
        secondsRemaining = secondsRemaining % 60;
        
        if (isConnected) {
            Log.info("Status: CONNECTED to device (waiting for data...) | GPS Timer: %d:%02d remaining", 
                     minutesRemaining, secondsRemaining);
        } else {
            Log.info("Status: NOT CONNECTED, scanning... | GPS Timer: %d:%02d remaining", 
                     minutesRemaining, secondsRemaining);
        }
    }
    
    // Auto-disconnect removed - now disconnects immediately after data transfer
    
    // Check for GPS update (replaces timer to avoid BLE conflicts)
    checkGPSUpdate();
    
    // BLE events are handled by callbacks
    delay(100);
}