#ifndef BLE_PERIPHERAL_H
#define BLE_PERIPHERAL_H

#include <bluefruit.h>

// BLE UUIDs
#define SERVICE_UUID                   "12345678-1234-1234-1234-123456789abc"
#define DATA_CHARACTERISTIC_UUID       "87654321-4321-4321-4321-cba987654321"
#define ACK_CHARACTERISTIC_UUID        "11223344-5566-7788-99aa-bbccddeeff00"

// Configuration
extern const char* DEVICE_NAME;
extern const int NUM_DATA_POINTS;
extern const unsigned long SEND_INTERVAL;

// Data structures
struct DataPoint {
  uint8_t val1;   // 0-255 (for your 0 or 1 values)
  uint32_t val2;  // 0 to 4,294,967,295 (handles your 0-65,545 range)
  uint32_t val3;  // 0 to 4,294,967,295 (handles your 0-65,545 range)
};

struct DataPacket {
  uint16_t packetNumber;    // Supports up to 65,535 packets
  uint16_t totalPackets;    // Supports up to 65,535 packets  
  uint8_t pointsInPacket;   // Points in this packet
  DataPoint points[1];      // 1 point per packet
};

// BLE objects
extern BLEService dataService;
extern BLECharacteristic dataCharacteristic;
extern BLECharacteristic ackCharacteristic;

// State variables
extern DataPoint dataBuffer[500];
extern bool isConnected;
extern bool isSending;
extern unsigned long lastSendTime;
extern uint32_t lastAckTimestamp;
extern bool hasNewTimestamp;

// Function declarations
void initBLE();
void setupService();
void startAdvertising();
void generateTestData();
void sendDataBatch();
void ackCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
void connectCallback(uint16_t conn_handle);
void disconnectCallback(uint16_t conn_handle, uint8_t reason);

// Main loop function
void handleBLELoop();

#endif