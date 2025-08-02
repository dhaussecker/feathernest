
#include "Particle.h"
#include "Arduino.h"
#include "ble.h"

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

    // Start scanning for devices
    delay(1000); // Give BLE a moment to settle


    if (startScanning()) {
        Log.info("Scanning for BLE devices...");
    } else {
        Log.error("Failed to start scanning!");
    }
}

// loop() runs over and over again, as quickly as it can execute.
void loop() 
{   
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
        Log.info("Test1.2");
    }
    
    // Watchdog: If scan has been running too long, something is wrong
    if (scanInProgress && (millis() - scanStartTime > 30000)) {
        Log.warn("BLE scan appears to be hanging - will retry in next cycle");
        scanInProgress = false;
        lastScanTime = millis() - 10000; // Retry sooner
    }
    
    // Show connection status periodically
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime >= 10000) {
        lastStatusTime = millis();
        
        if (isConnected) {
            Log.info("Status: CONNECTED to device (waiting for data...)");
        } else {
            Log.info("Status: NOT CONNECTED, scanning...");
        }
    }
    
    // Auto-disconnect removed - now disconnects immediately after data transfer
    
    // BLE events are handled by callbacks
    delay(100);
}