#include "ble.h"
#include "gpstime.h"

// Define the target device names
const char* TARGET_DEVICE_NAMES[] = {"nRF_01", "nRF_02", "nRF_03"};
const int NUM_TARGET_DEVICES = 3;

// Global variables
bool isScanning = false;
int scanCount = 0;
int currentTargetIndex = 0;  // Round-robin index for target devices
bool isConnected = false;
BlePeerDevice connectedDevice;
BleAddress targetDeviceAddress;
bool disconnectRequested = false;

// Global variables for data collection
// Pre-allocate buffer to prevent heap fragmentation
char dataBuffer[2048];  // Buffer for collected data
int dataBufferPos = 0;  // Current position in buffer
int nonZeroDataCount = 0;

// Service and characteristic UUIDs
BleUuid serviceUuid(SERVICE_UUID);
BleUuid dataCharUuid(DATA_CHARACTERISTIC_UUID);
BleUuid ackCharUuid(ACK_CHARACTERISTIC_UUID);

// Characteristic objects
BleCharacteristic dataCharacteristic;
BleCharacteristic ackCharacteristic;

bool initBLE() {
    Log.info("Initializing BLE...");
    
    // Turn on BLE module
    if (!BLE.on()) {
        Log.error("Failed to turn on BLE");
        return false;
    }
    
    // Wait a bit for BLE to be ready
    delay(1000);
    
    // Set device name for central
    BLE.setDeviceName("Particle_Central_01");
    
    // Set TX power to maximum for better range
    BLE.setTxPower(8); // Max power
    
    // Set up disconnection callback
    BLE.onDisconnected(onDisconnected, nullptr);
    
    Log.info("BLE initialized successfully");
    Log.info("Device name: Particle_Central_01");
    Log.info("Ready to scan for devices: %s, %s, %s", 
             TARGET_DEVICE_NAMES[0], 
             TARGET_DEVICE_NAMES[1], 
             TARGET_DEVICE_NAMES[2]);
    
    return true;
}

bool startScanning() {
    if (isScanning) {
        Log.info("Already scanning");
        return true;
    }
    
    // Show which device we're targeting this round
    const char* targetDevice = TARGET_DEVICE_NAMES[currentTargetIndex];
    Log.info("Starting BLE scan targeting: %s (device %d/%d)", 
             targetDevice, currentTargetIndex + 1, NUM_TARGET_DEVICES);
    
    // Reset scan count
    scanCount = 0;
    
    // Mark scan start time for duration tracking
    unsigned long scanStartTime = millis();
    
    // Use blocking scan but with a reasonable array size to prevent hanging
    BleScanResult results[20]; // Buffer for up to 20 results
    
    // Ensure WiFi doesn't interfere with BLE scanning
    // Small delay to let radio stabilize after any WiFi activity
    delay(100);
    
    // Use callback-based scanning with timeout to prevent hanging
    isScanning = true;
    Vector<BleScanResult> scanResults;
    
    // Start non-blocking scan with callback
    int scanResult = BLE.scan([&scanResults](const BleScanResult& result) {
        scanResults.append(result);
    });
    
    if (scanResult < 0) {
        Log.error("Failed to start BLE scan, error: %d", scanResult);
        isScanning = false;
        return false;
    }
    
    // Wait for scan with timeout (5 seconds max)
    unsigned long scanTimeout = 5000;
    unsigned long scanStart = millis();
    
    while (isScanning && (millis() - scanStart < scanTimeout)) {
        delay(100);
        Particle.process(); // Keep cloud connection alive
    }
    
    // Stop scanning if still running
    if (isScanning) {
        BLE.stopScanning();
        Log.warn("BLE scan timed out after %lu ms", scanTimeout);
    }
    
    isScanning = false;
    
    // Log scan duration
    unsigned long scanDuration = millis() - scanStartTime;
    Log.info("BLE scan took %lu ms", scanDuration);
    
    int resultCount = scanResults.size();
    
    Log.info("Scan complete. Found %d devices", scanResults.size());
    
    if (scanResults.size() == 0) {
        Log.info("No devices found - trying next target device");
        // Move to next target device in round-robin fashion
        currentTargetIndex = (currentTargetIndex + 1) % NUM_TARGET_DEVICES;
        return true; // Return success so we continue the cycle
    }
    
    // Process all found devices, but prioritize our current target
    bool foundTarget = false;
    for (int i = 0; i < scanResults.size(); i++) {
        if (processScanResult(&scanResults[i])) {
            foundTarget = true;
            break; // Stop after finding and connecting to target
        }
    }
    
    if (!foundTarget) {
        Log.info("Target device %s not found - trying next target", targetDevice);
        // Move to next target device
        currentTargetIndex = (currentTargetIndex + 1) % NUM_TARGET_DEVICES;
    }
    
    return true;
}

bool processScanResult(const BleScanResult* scanResult) {
    scanCount++;
    
    // Get device name from advertising data
    String deviceName = scanResult->advertisingData().deviceName();
    
    // Only process devices with names
    if (deviceName.length() == 0) {
        return false;
    }
    
    Log.info("Found device: %s (RSSI: %d)", deviceName.c_str(), scanResult->rssi());
    
    // Check if this matches our current target device
    const char* currentTarget = TARGET_DEVICE_NAMES[currentTargetIndex];
    
    if (deviceName == currentTarget || 
        (deviceName == "nRF_0" && (String(currentTarget).startsWith("nRF_0")))) {
        
        Log.info("*** FOUND CURRENT TARGET: %s ***", deviceName.c_str());
        Log.info("  Address: %02X:%02X:%02X:%02X:%02X:%02X", 
                 scanResult->address()[0], scanResult->address()[1], 
                 scanResult->address()[2], scanResult->address()[3], 
                 scanResult->address()[4], scanResult->address()[5]);
        Log.info("  RSSI: %d dBm", scanResult->rssi());
        
        // Save target device address for connection
        targetDeviceAddress = scanResult->address();
        
        // Try to connect immediately
        if (connectToDevice(scanResult)) {
            Log.info("Connection initiated successfully to %s!", currentTarget);
            return true; // Successfully found and connected to target
        } else {
            Log.error("Failed to initiate connection to %s", currentTarget);
            return false;
        }
    } else {
        // This is not our current target device, skip it
        Log.info("Device %s found but not current target (%s) - skipping", 
                 deviceName.c_str(), currentTarget);
        return false;
    }
}

void stopScanning() {
    // With blocking scan, there's no continuous scanning to stop
    // This function is here for completeness
    Log.info("stopScanning() called - using blocking scan mode");
}

bool connectToDevice(const BleScanResult* scanResult) {
    if (isConnected) {
        Log.warn("Already connected to a device");
        return false;
    }
    
    Log.info("Attempting to connect to device...");
    
    // Connect to the device
    connectedDevice = BLE.connect(scanResult->address());
    
    if (connectedDevice.connected()) {
        isConnected = true;
        Log.info("Successfully connected!");
        
        // Reset data collection for new device
        resetDataCollection();
        
        // Small delay to let connection stabilize
        delay(500);
        
        // Connection established - the peripheral will request extended timeout parameters
        Log.info("Connection established. Waiting for peripheral to negotiate timeout parameters...");
        
        // Give extra time for connection parameter negotiation to complete
        // The peripheral should request extended supervision timeout
        delay(1000);
        
        // Automatically discover services after connection
        Log.info("Starting service discovery...");
        
        if (discoverServices()) {
            Log.info("Service discovery complete! Ready to receive data.");
        } else {
            Log.error("Service discovery failed!");
            disconnectFromDevice();
            return false;
        }
        
        return true;
    } else {
        Log.error("Connection failed");
        return false;
    }
}

void disconnectFromDevice() {
    if (!isConnected) {
        Log.info("No device connected");
        return;
    }
    
    Log.info("Disconnecting from device...");
    
    if (connectedDevice.connected()) {
        connectedDevice.disconnect();
    }
    
    isConnected = false;
    Log.info("Disconnected");
}

bool discoverServices() {
    Log.info("Starting service discovery...");
    
    // Discover all services first
    connectedDevice.discoverAllServices();
    
    // Get services matching our UUID
    Vector<BleService> services = connectedDevice.getServiceByUUID(serviceUuid);
    
    if (services.size() == 0) {
        Log.error("Service not found! UUID: %s", SERVICE_UUID);
        return false;
    }
    
    Log.info("Found our service!");
    BleService service = services[0];
    
    // Discover all characteristics for this service
    connectedDevice.discoverAllCharacteristics();
    
    // Get data characteristic
    bool foundDataChar = connectedDevice.getCharacteristicByUUID(service, dataCharacteristic, dataCharUuid);
    if (!foundDataChar) {
        Log.error("Data characteristic not found! UUID: %s", DATA_CHARACTERISTIC_UUID);
        return false;
    }
    Log.info("Found data characteristic");
    Log.info("Data char UUID: %s", dataCharacteristic.UUID().toString().c_str());
    Log.info("Expected UUID: %s", DATA_CHARACTERISTIC_UUID);
    
    // Get ACK characteristic
    bool foundAckChar = connectedDevice.getCharacteristicByUUID(service, ackCharacteristic, ackCharUuid);
    if (!foundAckChar) {
        Log.error("ACK characteristic not found! UUID: %s", ACK_CHARACTERISTIC_UUID);
        return false;
    }
    Log.info("Found ACK characteristic");
    
    // Check if we can write to ACK characteristic
    // For now, assume ACK characteristic supports writing
    // The feather peripheral sets it up with WRITE property
    Log.info("ACK characteristic ready for writing");
    
    // Small delay before enabling notifications (like nRF52 implementation)
    delay(100);
    
    // Enable notifications for data characteristic
    if (enableNotifications()) {
        Log.info("Notifications enabled successfully!");
    } else {
        Log.error("Failed to enable notifications!");
        return false;
    }
    
    Log.info("All services and characteristics discovered successfully!");
    
    return true;
}

bool sendAckWithTimestamp() {
    if (!isConnected) {
        Log.error("Cannot send ACK - not connected");
        return false;
    }
    
    // Get current Unix timestamp
    uint32_t timestamp = Time.now();
    
    Log.info("Preparing ACK with timestamp: %lu", timestamp);
    
    // Write timestamp as 4 bytes (little-endian)
    uint8_t timestampBytes[4];
    timestampBytes[0] = (timestamp) & 0xFF;
    timestampBytes[1] = (timestamp >> 8) & 0xFF;
    timestampBytes[2] = (timestamp >> 16) & 0xFF;
    timestampBytes[3] = (timestamp >> 24) & 0xFF;
    
    // Check if ACK characteristic is valid before writing
    if (!ackCharacteristic.UUID().isValid()) {
        Log.error("ACK characteristic UUID is invalid!");
        return false;
    }
    
    Log.info("Writing %d bytes to ACK characteristic (UUID: %s)...", 
             sizeof(timestampBytes), ackCharacteristic.UUID().toString().c_str());
    
    // Track write timing for debugging
    unsigned long writeStartTime = millis();
    
    // Try the ACK write - this is where it may hang
    int result = ackCharacteristic.setValue(timestampBytes, sizeof(timestampBytes));
    
    unsigned long writeTime = millis() - writeStartTime;
    Log.info("ACK write completed in %lu ms, result: %d", writeTime, result);
    
    if (result == sizeof(timestampBytes)) {
        Log.info("ACK sent successfully with timestamp %lu", timestamp);
        return true;
    } else {
        Log.error("ACK write failed - result: %d (expected: %d)", result, sizeof(timestampBytes));
        return false;
    }
}

bool enableNotifications() {
    Log.info("Enabling notifications on data characteristic...");
    
    // First check if the characteristic supports notifications
    uint8_t properties = dataCharacteristic.properties();
    Log.info("Data characteristic properties: 0x%02X", properties);
    Log.info("Expected properties: READ (0x02) | NOTIFY (0x10) = 0x12");
    
    // Log individual properties
    if (properties & 0x01) Log.info("  - BROADCAST supported");
    if (properties & 0x02) Log.info("  - READ supported");
    if (properties & 0x04) Log.info("  - WRITE_WO_RSP supported");
    if (properties & 0x08) Log.info("  - WRITE supported");
    if (properties & 0x10) Log.info("  - NOTIFY supported");
    if (properties & 0x20) Log.info("  - INDICATE supported");
    
    // Set up the notification callback first
    dataCharacteristic.onDataReceived(onDataReceived, &dataCharacteristic);
    
    // Try to subscribe regardless of properties check
    // Some implementations don't report properties correctly
    Log.info("Attempting to subscribe to notifications despite properties...");
    
    // Enable notifications using subscribe
    // subscribe() returns the number of bytes written to CCCD, not a boolean
    int result = dataCharacteristic.subscribe(true);
    
    if (result == SYSTEM_ERROR_NONE || result > 0) {
        Log.info("Successfully subscribed to notifications (result: %d)", result);
        return true;
    } else {
        Log.error("Failed to subscribe to notifications, error: %d", result);
        
        // Try again with a delay
        delay(100);
        result = dataCharacteristic.subscribe(true);
        
        if (result == SYSTEM_ERROR_NONE || result > 0) {
            Log.info("Successfully subscribed on second attempt (result: %d)", result);
            return true;
        } else {
            Log.error("Second subscribe attempt also failed, error: %d", result);
            
            // Log more details about the characteristic
            Log.info("Characteristic UUID: %s", dataCharacteristic.UUID().toString().c_str());
            
            return false;
        }
    }
}

void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context) {
    static unsigned long lastPacketTime = millis();
    static int lastPacketNumber = 0;
    static int consecutiveTimeouts = 0;
    
    // Special case: reset static variables on new connection
    if (!isConnected) {
        lastPacketNumber = 0;
        consecutiveTimeouts = 0;
        lastPacketTime = millis();
        return;
    }
    
    Log.info("Data received! Length: %d bytes", len);
    
    // Check for packet timeout (if no packets received for 3 seconds, connection likely lost)
    if (millis() - lastPacketTime > 3000 && lastPacketNumber > 0) {
        consecutiveTimeouts++;
        Log.warn("Packet timeout detected - connection may be lost (timeout count: %d)", consecutiveTimeouts);
        
        // If we get 2 consecutive timeouts, force disconnect
        if (consecutiveTimeouts >= 2) {
            Log.error("Multiple timeouts detected - forcing disconnect");
            forceDisconnect();
            return;
        }
    } else {
        consecutiveTimeouts = 0;  // Reset timeout counter on successful packet
    }
    lastPacketTime = millis();
    
    // BLE adds some overhead, so accept packets between 14-20 bytes for our single-point packets
    if (len < 14 || len > 20) {
        Log.error("Invalid packet size: %d bytes", len);
        return;
    }
    
    // Parse the DataPacket
    DataPacket* packet = (DataPacket*)data;
    
    // Check for packet sequence issues
    if (lastPacketNumber > 0 && packet->packetNumber != lastPacketNumber + 1) {
        if (packet->packetNumber == 1) {
            Log.warn("Packet sequence restarted! Previous packet: %d, Current: %d", lastPacketNumber, packet->packetNumber);
            Log.warn("Peripheral may have reset - clearing data collection");
            resetDataCollection();
        } else {
            Log.error("Packet sequence error! Expected: %d, Received: %d", lastPacketNumber + 1, packet->packetNumber);
        }
    }
    
    // Update last packet tracking
    lastPacketNumber = packet->packetNumber;
    
    Log.info("Packet %d/%d received", packet->packetNumber, packet->totalPackets);
    Log.info("Points in packet: %d", packet->pointsInPacket);
    
    // For BLE notifications, the actual data might be padded to 20 bytes
    // but our packet structure is only 14 bytes for single point packets
    size_t minExpectedSize = 5 + (packet->pointsInPacket * sizeof(DataPoint));
    if (len < minExpectedSize) {
        Log.error("Packet too small. Expected at least: %d, Received: %d", minExpectedSize, len);
        return;
    }
    
    // Process each data point in the packet
    for (int i = 0; i < packet->pointsInPacket; i++) {
        DataPoint* point = &packet->points[i];
        Log.info("  Point %d: val1=%d, val2=%lu, val3=%lu", 
                 i + 1, point->val1, point->val2, point->val3);
        
        // Collect non-zero data points
        if (point->val1 != 0 || point->val2 != 0 || point->val3 != 0) {
            // Add to collected data buffer
            if (dataBufferPos > 0 && dataBufferPos < (int)(sizeof(dataBuffer) - 1)) {
                dataBuffer[dataBufferPos++] = ',';
            }
            
            // Format data point into buffer
            int written = snprintf(dataBuffer + dataBufferPos, 
                                 sizeof(dataBuffer) - dataBufferPos,
                                 "%d:%lu:%lu", point->val1, point->val2, point->val3);
            
            if (written > 0 && dataBufferPos + written < (int)sizeof(dataBuffer)) {
                dataBufferPos += written;
            }
            
            nonZeroDataCount++;
            Log.info("    Non-zero data point collected (total: %d)", nonZeroDataCount);
        }
    }
    
    // Check if this is the last packet
    if (packet->packetNumber == packet->totalPackets) {
        Log.info("All packets received! Total: %d", packet->totalPackets);
        
        Log.info("Data transfer complete. All packets received successfully!");
        
        // Publish collected non-zero data points immediately
        const char* deviceName = TARGET_DEVICE_NAMES[currentTargetIndex];
        publishCollectedData(deviceName);
        
        // DON'T send ACK or disconnect from callback - set flag for main loop
        Log.info("Queuing disconnect after data transfer complete");
        disconnectRequested = true;
        
    } else {
        Log.info("Waiting for more packets... (%d/%d)", packet->packetNumber, packet->totalPackets);
    }
}

void onDisconnected(const BlePeerDevice& peer, void* context) {
    Log.info("=== DISCONNECTED FROM PERIPHERAL ===");
    Log.info("Device address: %02X:%02X:%02X:%02X:%02X:%02X", 
             peer.address()[0], peer.address()[1], peer.address()[2], 
             peer.address()[3], peer.address()[4], peer.address()[5]);
    
    // Reset connection state
    isConnected = false;
    
    // Clear the connected device
    connectedDevice = BlePeerDevice();
    
    // Move to next target device for round-robin cycling
    const char* previousTarget = TARGET_DEVICE_NAMES[currentTargetIndex];
    currentTargetIndex = (currentTargetIndex + 1) % NUM_TARGET_DEVICES;
    const char* nextTarget = TARGET_DEVICE_NAMES[currentTargetIndex];
    
    Log.info("Data collection from %s complete. Next target: %s", previousTarget, nextTarget);
    Log.info("Connection state reset. Will resume scanning for next device...");
}

void forceDisconnect() {
    if (!isConnected) {
        Log.info("No device connected to disconnect");
        return;
    }
    
    Log.info("Forcing disconnection from device...");
    
    if (connectedDevice.connected()) {
        Log.info("Device reports as connected - sending disconnect command...");
        connectedDevice.disconnect();
        Log.info("Disconnect command sent");
        delay(100); // Give time for disconnect to process
    } else {
        Log.info("Device already reports as disconnected");
    }
    
    // Only NOW set connection state to false (after disconnect command)
    isConnected = false;
    Log.info("Connection state reset to false");
    
    // Clear the connected device object
    connectedDevice = BlePeerDevice();
    Log.info("Connected device object cleared");
    
    Log.info("Force disconnect complete - ready to resume scanning");
}

void resetDataCollection() {
    memset(dataBuffer, 0, sizeof(dataBuffer));
    dataBufferPos = 0;
    nonZeroDataCount = 0;
    Log.info("Data collection reset for new device");
}

void publishCollectedData(const char* deviceName) {
    if (nonZeroDataCount == 0) {
        Log.info("No non-zero data points to publish for %s", deviceName);
        return;
    }
    
    // Get current GPS data
    GPSData gpsData = getGPSData();
    
    // Create JSON-like event data with GPS info
    String eventData;
    if (gpsData.valid) {
        // Null terminate the data buffer
        dataBuffer[dataBufferPos] = '\0';
        eventData = String::format("{\"lat\":%.6f,\"lon\":%.6f,\"time\":%lld,\"device\":\"%s\",\"count\":%d,\"data\":\"%s\"}", 
                                 gpsData.latitude, gpsData.longitude, (long long)gpsData.timestamp,
                                 deviceName, nonZeroDataCount, dataBuffer);
    } else {
        // If GPS not available, use 0 values
        // Null terminate the data buffer
        dataBuffer[dataBufferPos] = '\0';
        eventData = String::format("{\"lat\":0.0,\"lon\":0.0,\"time\":%lld,\"device\":\"%s\",\"count\":%d,\"data\":\"%s\"}", 
                                 (long long)Time.now(),
                                 deviceName, nonZeroDataCount, dataBuffer);
    }
    
    Log.info("Publishing %d non-zero data points from %s to Particle Cloud", nonZeroDataCount, deviceName);
    Log.info("Event data: %s", eventData.c_str());
    
    // Publish to Particle Cloud with event name "state"
    bool published = Particle.publish("state", eventData, PRIVATE);
    
    if (published) {
        Log.info("Successfully published data to Particle Cloud event 'state'");
    } else {
        Log.error("Failed to publish data to Particle Cloud");
    }
}