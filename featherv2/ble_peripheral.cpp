#include "BLE_Peripheral.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
extern void loadDataFromFlash(DataPoint* dataBuffer, int numDataPoints);


// Configuration
const char* DEVICE_NAME = "nRF_01";
const int NUM_DATA_POINTS = 100;
const unsigned long SEND_INTERVAL = 10000; // 10 seconds

// BLE objects
BLEService dataService(SERVICE_UUID);
BLECharacteristic dataCharacteristic(DATA_CHARACTERISTIC_UUID);
BLECharacteristic ackCharacteristic(ACK_CHARACTERISTIC_UUID);

// State variables
DataPoint dataBuffer[500];
bool isConnected = false;
bool isSending = false;
unsigned long lastSendTime = 0;
uint32_t lastAckTimestamp = 0;
bool hasNewTimestamp = false;

void initBLE() {
  Bluefruit.begin();
  Bluefruit.setTxPower(0);
  Bluefruit.setName(DEVICE_NAME);
  
  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);
}

void setupService() {
  dataService.begin();
  
  dataCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  dataCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  dataCharacteristic.begin();
  
  ackCharacteristic.setProperties(CHR_PROPS_WRITE);
  ackCharacteristic.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  ackCharacteristic.setWriteCallback(ackCallback);
  ackCharacteristic.begin();
}

void startAdvertising() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(dataService);
  Bluefruit.Advertising.addName();
  
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void generateTestData() {
  for (int i = 0; i < NUM_DATA_POINTS; i++) {
    dataBuffer[i].val1 = random(0, 2);        // 0 or 1
    dataBuffer[i].val2 = random(0, 65546);    // 0 to 65,545 range
    dataBuffer[i].val3 = random(0, 65546);    // 0 to 65,545 range
  }
}



void sendDataBatch() {
  if (!isConnected || isSending) return;
  
  isSending = true;
  
  loadDataFromFlash(dataBuffer, 5);
  
  const int totalPackets = NUM_DATA_POINTS;
  
  for (int packet = 0; packet < totalPackets; packet++) {
    if (!isConnected) {
      isSending = false;
      return;
    }
    
    DataPacket dataPacket;
    dataPacket.packetNumber = packet + 1;
    dataPacket.totalPackets = totalPackets;
    dataPacket.pointsInPacket = 1;
    dataPacket.points[0] = dataBuffer[packet];
    
    if (!dataCharacteristic.notify((uint8_t*)&dataPacket, sizeof(DataPacket))) {
      isSending = false;
      return;
    }
    
    delay(20);
  }
}

void ackCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  if (len == sizeof(uint32_t)) {
    lastAckTimestamp = *(uint32_t*)data;  // Store the timestamp
    hasNewTimestamp = true;               // Mark as new (optional)
    
    Serial.print("ACK received with timestamp: ");
    Serial.println(lastAckTimestamp);
    
    isSending = false;
  }
}

void connectCallback(uint16_t conn_handle) {
  isConnected = true;
  lastSendTime = millis();
}

void disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  isConnected = false;
  isSending = false;
}

void handleBLELoop() {
  if (isConnected && !isSending && (millis() - lastSendTime >= SEND_INTERVAL)) {
    sendDataBatch();
    lastSendTime = millis();
  }
}