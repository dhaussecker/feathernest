#include "accelerometernew.h"
#include "BLE_Peripheral.h"
#include "flash_storage.h"
#include "rtc_clock.h"

bool timeSync = false;
int32_t previousTime = -1;

// STAGE 3 VARIABLES
unsigned long BLE_ATTEMPT_INTERVAL = 480; // 60 seconds * 8Hz
unsigned long lastBLEAttempt = 0;
bool stage3Active = false;
uint32_t oldTimeStamp = 0;


bool timeStampStored = false;

void setup() {
  
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
  if (timeStampStored && motionDetected && !stage3Active) {
    motionDetected = false;
    Serial.println("STAGE 2");
    int32_t time = readRTC();
    //int state = checkForStateChange();
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
      Serial.println("DATA SENT - CYCLE COMPLETE");
      stage3Active = false;
      // Clear old data files
      deleteLogs("/time.txt");
      deleteLogs("/state.txt");
      // Reset to wait for NEW timestamp from central
      hasNewTimestamp = false;  // Wait for fresh timestamp
      timeStampStored = false;
      resetRTC();
      Serial.println("Waiting for new timestamp from central...");
    }
/*    else if (oldTimeStamp == lastAckTimestamp && readRTC()-lastBLEAttempt >= 240 && sentFlag == true)
    {
      Serial.println("30 SECOND TIMEOUT");
      stage3Active = false;
      sentFlag = false;
      BLE_ATTEMPT_INTERVAL += 720;
    }*/

  }
  
  
  delay(1000);
}