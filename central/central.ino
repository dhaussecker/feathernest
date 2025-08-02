#include <bluefruit.h>

// BLE UUIDs (must match peripheral)
#define SERVICE_UUID                   "12345678-1234-1234-1234-123456789abc"
#define DATA_CHARACTERISTIC_UUID       "87654321-4321-4321-4321-cba987654321"
#define ACK_CHARACTERISTIC_UUID        "11223344-5566-7788-99aa-bbccddeeff00"

// Target device names - can match multiple peripherals
const char* TARGET_DEVICE_NAMES[] = {"nRF_01", "nRF_02", "nRF_03"};
const int NUM_TARGET_DEVICES = 3;

// BLE client objects
BLEClientService        dataService(SERVICE_UUID);
BLEClientCharacteristic dataCharacteristic(DATA_CHARACTERISTIC_UUID);
BLEClientCharacteristic ackCharacteristic(ACK_CHARACTERISTIC_UUID);

// Data structures (must match peripheral)
struct DataPoint {
  uint8_t val1;   // 8 bits: 0-255 (for your 0 or 1 values)
  uint32_t val2;  // 32 bits: 0 to 4,294,967,295 (handles your 0-65,545 range)
  uint32_t val3;  // 32 bits: 0 to 4,294,967,295 (handles your 0-65,545 range)
};

struct DataPacket {
  uint16_t packetNumber;    // 2 bytes - supports up to 65,535 packets
  uint16_t totalPackets;    // 2 bytes - supports up to 65,535 packets
  uint8_t pointsInPacket;   // 1 byte
  DataPoint points[1];      // 1 point = 9 bytes, total packet = 14 bytes
};

// Reception state
DataPoint receivedData[500];  // Can handle up to 500 points
int totalReceivedPoints = 0;
int expectedPackets = 0;
int receivedPackets = 0;
bool isReceiving = false;
int scanCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("BLE Central - Data Receiver");
  
  // Initialize BLE
  Bluefruit.begin(0, 1); // 0 peripheral, 1 central
  Bluefruit.setName("nRF_Central_01");
  
  // Set up connection callbacks
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);
  
  // Set up client service and characteristics
  dataService.begin();
  
  dataCharacteristic.begin();
  dataCharacteristic.setNotifyCallback(data_notify_callback);
  
  ackCharacteristic.begin();
  
  Serial.println("BLE initialized, starting scan...");
  
  // Start scanning
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.useActiveScan(true);  // Try active scanning
  
  if (Bluefruit.Scanner.start(0)) {
    Serial.println("Scanner started successfully!");
  } else {
    Serial.println("Failed to start scanner!");
  }
  
  Serial.println("Scanning for any BLE devices...");
}

void loop() {
  // Check if we should be scanning but aren't
  static unsigned long lastScanCheck = 0;
  static int lastScanCount = 0;
  
  if (millis() - lastScanCheck >= 5000) { // Check every 5 seconds
    bool isConnected = Bluefruit.Central.connected();
    bool isScanning = Bluefruit.Scanner.isRunning();
    
    // Print status for debugging
    static int statusCount = 0;
    if (statusCount % 4 == 0) { // Print every 20 seconds (5s * 4)
      Serial.print("Status - Connected: ");
      Serial.print(isConnected ? "YES" : "NO");
      Serial.print(", Scanning: ");
      Serial.print(isScanning ? "YES" : "NO");
      Serial.print(", Scan count: ");
      Serial.println(scanCount);
    }
    statusCount++;
    
    // Check if scanner is stuck (not receiving callbacks)
    if (!isConnected && isScanning && (scanCount == lastScanCount)) {
      Serial.println("Scanner appears stuck (no new callbacks) - forcing restart...");
      Bluefruit.Scanner.stop();
      delay(500);
      
      if (Bluefruit.Scanner.start(0)) {
        Serial.println("Scanner force-restarted successfully");
      } else {
        Serial.println("Scanner force-restart failed");
      }
    }
    
    // If not scanning and not connected, restart scanner
    if (!isScanning && !isConnected) {
      Serial.println("Scanner not running and not connected - restarting scan...");
      if (Bluefruit.Scanner.start(0)) {
        Serial.println("Scanner restarted in loop");
      } else {
        Serial.println("Failed to restart scanner in loop");
      }
    }
    
    lastScanCount = scanCount;
    lastScanCheck = millis();
  }
  
  // Main loop - BLE events are handled by callbacks
  delay(100);
}

void scan_callback(ble_gap_evt_adv_report_t* report) {
  scanCount++;
  
  // Print scan activity more frequently to see if callbacks are happening
  if (scanCount % 5 == 1) {  // Print every 5th scan instead of 10th
    Serial.print("Scan callback called, count: ");
    Serial.println(scanCount);
  }
  
  // Parse the advertising data to find device name
  uint8_t nameBuffer[32];
  uint8_t nameLen = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, nameBuffer, sizeof(nameBuffer));
  
  if (nameLen > 0) {
    // Null terminate the string
    nameBuffer[min(nameLen, 31)] = '\0';
    
    // Debug: print ALL found devices to see what's being detected
    Serial.print("Found device (complete name): ");
    Serial.println((char*)nameBuffer);
    
    // Check if complete name matches any of our targets
    for (int i = 0; i < NUM_TARGET_DEVICES; i++) {
      if (strcmp((char*)nameBuffer, TARGET_DEVICE_NAMES[i]) == 0) {
        Serial.print("*** FOUND TARGET DEVICE: ");
        Serial.println((char*)nameBuffer);
        
        // Stop scanning and connect
        Serial.println("Stopping scanner...");
        Bluefruit.Scanner.stop();
        Serial.println("Attempting connection...");
        
        if (Bluefruit.Central.connect(report)) {
          Serial.println("Connection initiated successfully");
        } else {
          Serial.println("Failed to initiate connection - resuming scan...");
          delay(500);
          Bluefruit.Scanner.start(0);
        }
        return;
      }
    }
  }
  
  // Try short name if complete name not found
  nameLen = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, nameBuffer, sizeof(nameBuffer));
  if (nameLen > 0) {
    nameBuffer[min(nameLen, 31)] = '\0';
    Serial.print("Found device (short name): ");
    Serial.println((char*)nameBuffer);
    
    // Check if short name matches any of our targets (for truncated names)
    for (int i = 0; i < NUM_TARGET_DEVICES; i++) {
      // Check first 5 characters for truncated names like "nRF_0" from "nRF_01"
      if (strncmp((char*)nameBuffer, TARGET_DEVICE_NAMES[i], min(5, strlen(TARGET_DEVICE_NAMES[i]))) == 0) {
        Serial.print("*** FOUND TARGET DEVICE BY SHORT NAME: ");
        Serial.print((char*)nameBuffer);
        Serial.print(" (matches ");
        Serial.print(TARGET_DEVICE_NAMES[i]);
        Serial.println(")");
        
        // Stop scanning and connect
        Serial.println("Stopping scanner...");
        Bluefruit.Scanner.stop();
        Serial.println("Attempting connection...");
        
        if (Bluefruit.Central.connect(report)) {
          Serial.println("Connection initiated successfully");
        } else {
          Serial.println("Failed to initiate connection - resuming scan...");
          delay(500);
          Bluefruit.Scanner.start(0);
        }
        return;
      }
    }
  }
}

void connect_callback(uint16_t conn_handle) {
  Serial.println("=== CENTRAL CONNECTION CALLBACK CALLED ===");
  Serial.print("Connection handle: ");
  Serial.println(conn_handle);
  
  // Check if connection is valid
  if (conn_handle == BLE_CONN_HANDLE_INVALID) {
    Serial.println("ERROR: Invalid connection handle!");
    return;
  }
  
  Serial.println("Connected to peripheral!");
  
  // Small delay before service discovery
  delay(100);
  
  // Discover services
  Serial.println("Starting service discovery...");
  if (dataService.discover(conn_handle)) {
    Serial.println("Data service discovered");
    
    // Small delay before characteristic discovery
    delay(100);
    
    // Discover characteristics
    Serial.println("Starting characteristic discovery...");
    if (dataCharacteristic.discover() && ackCharacteristic.discover()) {
      Serial.println("Characteristics discovered");
      
      // Small delay before enabling notifications
      delay(100);
      
      // Enable notifications for data characteristic
      Serial.println("Enabling notifications...");
      if (dataCharacteristic.enableNotify()) {
        Serial.println("Data notifications enabled");
        Serial.println("=== CENTRAL READY TO RECEIVE DATA ===");
      } else {
        Serial.println("Failed to enable notifications");
      }
    } else {
      Serial.println("Failed to discover characteristics");
    }
  } else {
    Serial.println("Failed to discover service");
  }
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  Serial.println("=== CENTRAL DISCONNECT CALLBACK ===");
  Serial.print("Disconnect reason: ");
  Serial.println(reason);
  
  // Reset reception state
  isReceiving = false;
  totalReceivedPoints = 0;
  receivedPackets = 0;
  expectedPackets = 0;
  
  Serial.println("Disconnected from peripheral");
  
  // Small delay before resuming scan
  delay(1000);
  
  // Resume scanning immediately with multiple retry attempts
  Serial.println("Automatically resuming scanning...");
  for (int retry = 0; retry < 3; retry++) {
    if (Bluefruit.Scanner.start(0)) {
      Serial.println("Scanner restarted successfully!");
      return;
    } else {
      Serial.print("Scanner restart attempt ");
      Serial.print(retry + 1);
      Serial.println(" failed - retrying...");
      delay(1000);
    }
  }
  Serial.println("All scanner restart attempts failed!");
}

void data_notify_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len) {
  // BLE adds some overhead, so accept packets between 16-20 bytes
  if (len >= 16 && len <= 20) {
    DataPacket* packet = (DataPacket*)data;
    
    Serial.print("Received packet ");
    Serial.print(packet->packetNumber);
    Serial.print("/");
    Serial.print(packet->totalPackets);
    Serial.print(" - Point: (");
    Serial.print(packet->points[0].val1);
    Serial.print(", ");
    Serial.print(packet->points[0].val2);
    Serial.print(", ");
    Serial.print(packet->points[0].val3);
    Serial.println(")");
    
    // First packet - initialize reception
    if (packet->packetNumber == 1) {
      isReceiving = true;
      expectedPackets = packet->totalPackets;
      receivedPackets = 0;
      totalReceivedPoints = 0;
      Serial.println("Starting batch reception...");
    }
    
    if (isReceiving) {
      // Store the data point
      if (totalReceivedPoints < 500) {  // Can handle up to 500 points
        receivedData[totalReceivedPoints] = packet->points[0];
        totalReceivedPoints++;
      } else {
        Serial.println("Warning: Received more than 500 points, ignoring excess");
      }
      
      receivedPackets++;
      
      // Check if we've received all packets
      if (receivedPackets >= expectedPackets) {
        Serial.println("All packets received!");
        processBatchData();
        sendAck();
        isReceiving = false;
      }
    }
  } else {
    Serial.print("Invalid packet size! Expected 16-20 bytes, got ");
    Serial.print(len);
    Serial.println(" bytes");
    
    // Print packet contents for debugging
    Serial.print("Packet contents: ");
    for (int i = 0; i < min(len, 20); i++) {
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}

void processBatchData() {
  Serial.println("=== BATCH DATA RECEIVED ===");
  Serial.print("Total points received: ");
  Serial.println(totalReceivedPoints);
  
  // Display first few and last few data points as examples
  Serial.println("First 5 data points:");
  for (int i = 0; i < min(5, totalReceivedPoints); i++) {
    Serial.print("Point ");
    Serial.print(i + 1);
    Serial.print(": (");
    Serial.print(receivedData[i].val1);
    Serial.print(", ");
    Serial.print(receivedData[i].val2);
    Serial.print(", ");
    Serial.print(receivedData[i].val3);
    Serial.println(")");
  }
  
  if (totalReceivedPoints > 5) {
    Serial.println("...");
    Serial.println("Last 5 data points:");
    for (int i = max(0, totalReceivedPoints - 5); i < totalReceivedPoints; i++) {
      Serial.print("Point ");
      Serial.print(i + 1);
      Serial.print(": (");
      Serial.print(receivedData[i].val1);
      Serial.print(", ");
      Serial.print(receivedData[i].val2);
      Serial.print(", ");
      Serial.print(receivedData[i].val3);
      Serial.println(")");
    }
  }
  
  Serial.println("=== END BATCH ===");
}

void sendAck() {
  // Generate fake unix timestamp
  uint32_t fakeTimestamp = millis() / 1000 + 1640995200; // Fake epoch time
  
  if (ackCharacteristic.write((uint8_t*)&fakeTimestamp, sizeof(fakeTimestamp))) {
    Serial.print("ACK sent with timestamp: ");
    Serial.println(fakeTimestamp);
  } else {
    Serial.println("Failed to send ACK");
  }
}