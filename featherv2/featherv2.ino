#include "accelerometer.h"
#include "BLE_Peripheral.h"
#include "flash_storage.h"
#include "rtc_clock.h"

bool timeSync = false;
uint8_t state = 1;
int32_t previousTime = -1;

// STAGE 3 VARIABLES
const unsigned long BLE_ATTEMPT_INTERVAL = 480; // 60 seconds * 8Hz
unsigned long lastBLEAttempt = 0;
bool stage3Active = false;
uint32_t oldTimeStamp = 0;


bool timeStampStored = false;

void setup() {
  
  pinMode(INT1_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INT1_PIN), handleMotionInterrupt, RISING);
  Wire.begin();
  Serial.begin(115200);
  
  
  Serial.println("BLE Peripheral - Data Sender");
  
  // Initialize accelerometer
  setupLSM6DSOX();

  // Initialize RTC
  initRTC();
  
  // Initialize flash storage
  configureFlash();
  deleteLogs("/time.txt");
  readLogs("/time.txt");
  deleteLogs("/state.txt");

  // Initialize BLE
  initBLE();
  setupService();
  startAdvertising();
  
  Serial.println("Ready to send data");
}

void loop() {

  //STAGE 1: FEATHER LOCALLY STORES A TIMESTAMP FROM A NEST DEVICE TO FLASH
  // Handle BLE operations
  if (!hasNewTimestamp) 
  {
    Serial.print("Latest timestamp: ");
    Serial.println(lastAckTimestamp);
    handleBLELoop();
  }
  if (hasNewTimestamp && !timeStampStored) 
  {
      Serial.println("WE ARE IN HERE");
      Serial.println(lastAckTimestamp);
      deleteLogs("/time.txt");
      writeLogEntry(lastAckTimestamp, 0, 0, 0, "/time.txt", 0);
      previousTime = 0;
      resetRTC();
      readLogs("/time.txt");
      timeStampStored = true;
  }

  // STAGE 2: Feather begins to collect equipment state data with time relative to timestamp
  if (timeStampStored && motionDetected) {
    Serial.println("STAGE 2");
    motionDetected = false;
    int32_t time = readRTC();
    writeLogEntry(0, state, previousTime, time, "/state.txt", 1);
    previousTime = time;
    Serial.println("=== Motion Event Logged ===");
    readLogs("/state.txt");
    Serial.println("==========================");
  }

  // STAGE 3: Every 1 minute attempt to find a Nest to connect to and send the data
  if (readRTC() >= BLE_ATTEMPT_INTERVAL && timeStampStored && !stage3Active)
  {
    Serial.println("STAGE 3 STARTED");
    stage3Active = true;
    oldTimeStamp = lastAckTimestamp;
    lastBLEAttempt = readRTC();
  }
  if (stage3Active)
  {
    Serial.print("STAGE 3 ACTIVE: ");
    Serial.print(lastAckTimestamp);
    Serial.print(",");
    Serial.println(oldTimeStamp);
    handleBLELoop();
    if (oldTimeStamp != lastAckTimestamp)
    {
      Serial.println("DATA SENT");
      stage3Active = false;
      // Clear old data files
      deleteLogs("/time.txt");
      deleteLogs("/state.txt");
      hasNewTimestamp = true;
      timeStampStored = false;
      resetRTC();
    }
    else if (oldTimeStamp == lastAckTimestamp && readRTC()-lastBLEAttempt >= 240)
    {
      Serial.println("30 SECOND TIMEOUT");
      stage3Active = false;
    }

  }
  
  
  delay(1000);
}