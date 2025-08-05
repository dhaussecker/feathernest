#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

// File for logging
extern File logFile(InternalFS);
const char* LOG_FILENAME = "/statelog.txt";

// Data structure
struct StateLog {
  uint32_t startTime;
  uint32_t endTime;
  uint8_t state;  // 0, 1, or 2
};

// Current session info
uint32_t sessionStartTime = 0;
uint8_t currentState = 0;


void loadDataFromFlash(DataPoint* dataBuffer, int numDataPoints) {
  Serial.println("Loading motion data from flash...");
  memset(dataBuffer, 0, sizeof(DataPoint) * numDataPoints);
  
  int dataCount = 0;
  uint32_t baseTimestamp = 0;
  
  // Read base timestamp from time.txt
  logFile = InternalFS.open("/time.txt", FILE_O_READ);
  if (logFile) {
    String timestampLine = logFile.readStringUntil('\n');
    baseTimestamp = timestampLine.toInt();
    logFile.close();
    Serial.print("Base timestamp: ");
    Serial.println(baseTimestamp);
  }
    // Read state log file
  logFile = InternalFS.open("/state.txt", FILE_O_READ);
  if (!logFile) {
    Serial.println("No state data found, using zeros");
    return;
  }
  
  // Parse each line from state.txt
  while (logFile.available() && dataCount < numDataPoints) {
    String line = logFile.readStringUntil('\n');
    line.trim();
  
      if (line.length() > 0) {
      // Parse CSV format: startTime,endTime,state
      int firstComma = line.indexOf(',');
      int secondComma = line.indexOf(',', firstComma + 1);
      
      if (firstComma != -1 && secondComma != -1) {
        uint32_t startTime = line.substring(0, firstComma).toInt();
        uint32_t endTime = line.substring(firstComma + 1, secondComma).toInt();
        uint8_t state = line.substring(secondComma + 1).toInt();
        
        // Convert to DataPoint format
        dataBuffer[dataCount].val1 = state;                        // Motion state
        dataBuffer[dataCount].val2 = baseTimestamp + (startTime / 8); // Absolute start time (convert 8Hz ticks to seconds)
        dataBuffer[dataCount].val3 = baseTimestamp + (endTime / 8);   // Absolute end time (convert 8Hz ticks to seconds)
        dataCount++;
        
        Serial.print("Loaded motion event: State=");
        Serial.print(state);
        Serial.print(", Start=");
        Serial.print(startTime);
        Serial.print(", End=");
        Serial.println(endTime);
      }
    }
  }
  
  logFile.close();
  
  Serial.print("Loaded ");
  Serial.print(dataCount);
  Serial.println(" motion events from flash");
  
// Fill remaining slots with actual zeros
for (int i = dataCount; i < numDataPoints; i++) {
  dataBuffer[i].val1 = 0;
  dataBuffer[i].val2 = 0;  // Change this to 0
  dataBuffer[i].val3 = 0;  // Change this to 0
}
  
}
// Function declarations
void configureFlash()
{
  // Initialize flash
  if (!InternalFS.begin()) {
    Serial.println("ERROR: Failed to initialize flash!");
    while (1) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(100);
    }
  }
}

bool writeLog(StateLog &entry, const char* filename = LOG_FILENAME) {
  logFile = InternalFS.open(filename, FILE_O_WRITE);
  if (!logFile) {
    Serial.println("ERROR: Failed to open file for writing!");
    return false;
  }
  
  // Move to end of file for append
  logFile.seek(logFile.size());
  
  // Write as CSV: startTime,endTime,state
  logFile.print(entry.startTime);
  logFile.print(",");
  logFile.print(entry.endTime);
  logFile.print(",");
  logFile.println(entry.state);
  
  logFile.flush();  // Force write to flash
  logFile.close();
  return true;
}

void readLogs(const char* filename = LOG_FILENAME) {
  Serial.println("\n=== Previous Session Logs ===");
  
  logFile = InternalFS.open(filename, FILE_O_READ);
  if (!logFile) {
    Serial.println("No previous logs found");
    return;
  }
  
  int entryCount = 0;
  while (logFile.available()) {
    String line = logFile.readStringUntil('\n');
    if (line.length() > 0) {
      Serial.println(line);
      entryCount++;
    }
  }
}

void deleteLogs(const char* filename = LOG_FILENAME) {
  InternalFS.remove(filename);
}

// Generic write function - writes any string data to file
bool writeData(const char* data, const char* filename = LOG_FILENAME, bool append = true) {
  logFile = InternalFS.open(filename, append ? FILE_O_WRITE : FILE_O_WRITE | LFS_O_TRUNC);
  if (!logFile) {
    Serial.println("ERROR: Failed to open file for writing!");
    return false;
  }
  
  if (append) {
    logFile.seek(logFile.size());
  }
  
  logFile.println(data);
  logFile.flush();
  logFile.close();
  return true;
}


// Write Unix timestamp
bool writeUnixTimestamp(uint32_t unixTime, const char* filename = LOG_FILENAME) {
  char buffer[50];
  snprintf(buffer, sizeof(buffer), "//TIMESTAMP: %lu", unixTime);
  return writeData(buffer, filename);
}

// Flexible log entry function - can write different formats
// Format types: 0 = Unix timestamp only, 1 = state/start/end, 2 = both
bool writeLogEntry(uint32_t timestamp, uint8_t state, uint32_t startTime, 
                   uint32_t endTime, const char* filename = LOG_FILENAME, 
                   uint8_t format = 1) {
  char buffer[200];
  
  switch(format) {
    case 0:  // Unix timestamp only
      snprintf(buffer, sizeof(buffer), "%lu", timestamp);
      break;
    case 1:  // State log format (original)
      snprintf(buffer, sizeof(buffer), "%lu,%lu,%u", startTime, endTime, state);
      break;
    case 2:  // Combined: Unix timestamp + state data
      snprintf(buffer, sizeof(buffer), "%lu,%lu,%lu,%u", timestamp, startTime, endTime, state);
      break;
    case 3:  // Custom format with labels
      snprintf(buffer, sizeof(buffer), "TS:%lu,START:%lu,END:%lu,STATE:%u", 
               timestamp, startTime, endTime, state);
      break;
    default:
      return false;
  }
  
  return writeData(buffer, filename);
}

#endif // FLASH_STORAGE_H