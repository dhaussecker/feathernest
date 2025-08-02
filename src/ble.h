#ifndef BLE_H
#define BLE_H

#include "Particle.h"

// BLE UUIDs (must match peripheral)
#define SERVICE_UUID                   "12345678-1234-1234-1234-123456789abc"
#define DATA_CHARACTERISTIC_UUID       "87654321-4321-4321-4321-cba987654321"
#define ACK_CHARACTERISTIC_UUID        "11223344-5566-7788-99aa-bbccddeeff00"

// Target device names - can match multiple peripherals
extern const char* TARGET_DEVICE_NAMES[];
extern const int NUM_TARGET_DEVICES;

// Data structures (must match peripheral)
struct DataPoint {
    uint8_t val1;   // 8 bits: 0-255
    uint32_t val2;  // 32 bits: 0 to 4,294,967,295
    uint32_t val3;  // 32 bits: 0 to 4,294,967,295
};

struct DataPacket {
    uint16_t packetNumber;    // 2 bytes
    uint16_t totalPackets;    // 2 bytes
    uint8_t pointsInPacket;   // 1 byte
    DataPoint points[1];      // 1 point = 9 bytes, total packet = 14 bytes
};

// Global variables for scanning
extern bool isScanning;
extern int scanCount;
extern int currentTargetIndex;

// Global variables for connection
extern bool isConnected;
extern BlePeerDevice connectedDevice;
extern BleAddress targetDeviceAddress;

// Global variables for data collection
extern String collectedDataString;
extern int nonZeroDataCount;

// Global variables for services and characteristics
extern BleUuid serviceUuid;
extern BleUuid dataCharUuid;
extern BleUuid ackCharUuid;
extern BleCharacteristic dataCharacteristic;
extern BleCharacteristic ackCharacteristic;

// Function declarations
bool initBLE();
bool startScanning();
void stopScanning();
bool processScanResult(const BleScanResult* scanResult);
bool connectToDevice(const BleScanResult* scanResult);
void disconnectFromDevice();
bool discoverServices();
bool sendAckWithTimestamp();
bool enableNotifications();
void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onDisconnected(const BlePeerDevice& peer, void* context);
void forceDisconnect();
void resetDataCollection();
void publishCollectedData(const char* deviceName);

#endif // BLE_H