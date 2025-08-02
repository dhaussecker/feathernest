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
  
  Serial.println("Starting data transmission...");
  
  for (int packet = 0; packet < totalPackets; packet++) {
    if (!isConnected) {
      Serial.print("Connection lost during transmission at packet ");
      Serial.println(packet + 1);
      isSending = false;
      return;
    }
    
    DataPacket dataPacket;
    dataPacket.packetNumber = packet + 1;
    dataPacket.totalPackets = totalPackets;
    dataPacket.pointsInPacket = 1;
    dataPacket.points[0] = dataBuffer[packet];
    
    bool notifyResult = dataCharacteristic.notify((uint8_t*)&dataPacket, sizeof(DataPacket));
    if (!notifyResult) {
      Serial.print("Notify failed at packet ");
      Serial.println(packet + 1);
      Serial.println("Connection may have been lost during transmission");
      isSending = false;
      return;
    }
    
    // Check connection status periodically during transmission
    if ((packet + 1) % 10 == 0) {
      Serial.print("Transmission progress: ");
      Serial.print(packet + 1);
      Serial.print("/");
      Serial.print(totalPackets);
      Serial.print(" - Connection active: ");
      Serial.println(isConnected ? "YES" : "NO");
    }
    
    // Print progress every 50 packets
    if ((packet + 1) % 50 == 0) {
      Serial.print("Sent packet ");
      Serial.print(packet + 1);
      Serial.print("/");
      Serial.println(totalPackets);
    }
    
    delay(150);  // 150ms delay: 100 packets × 150ms = 15 seconds (well under 32s timeout)
  }
  
  Serial.println("All packets sent successfully, waiting for ACK...");
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
  
  Serial.println("Connection established, configuring parameters...");
  
  // Request maximum supervision timeout for data transmission
  // 100 packets × 150ms = 15 seconds, safely under 32s timeout limit
  Serial.println("Requesting maximum supervision timeout (15s transmission time)...");
  
  // Use maximum allowed supervision timeout with very conservative intervals
  bool result1 = Bluefruit.Gap.requestConnParams(conn_handle, 
    50,   // min_conn_interval (62.5ms) - very conservative
    100,  // max_conn_interval (125ms) - very conservative  
    0,    // slave_latency
    3200  // supervision_timeout (32000ms = 32 seconds maximum)
  );
  
  Serial.print("First connection parameter request: ");
  Serial.println(result1 ? "SUCCESS" : "FAILED");
  
  delay(500);
  
  // Try setting connection parameters directly as well 
  bool result2 = Bluefruit.Gap.setConnParams(40, 80, 0, 3200);
  Serial.print("Direct connection parameter set: ");
  Serial.println(result2 ? "SUCCESS" : "FAILED");
  
  // Longer delay to ensure parameters are applied
  delay(2000);
  
  Serial.println("Connection configured with maximum timeout - ready for transmission");
}

void disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Serial.print("PERIPHERAL DISCONNECTED - Reason: ");
  Serial.println(reason);
  
  isConnected = false;
  isSending = false;
}

void handleBLELoop() {
  if (isConnected && !isSending && (millis() - lastSendTime >= SEND_INTERVAL)) {
    sendDataBatch();
    lastSendTime = millis();
  }
}