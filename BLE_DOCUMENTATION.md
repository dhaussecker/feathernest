# BLE Central Implementation Documentation

## Overview
The `ble.cpp` file implements a BLE (Bluetooth Low Energy) central device that connects to peripheral devices in a round-robin fashion, collects data packets, and publishes the collected data to the Particle Cloud.

## Key Features
- **Round-robin scanning**: Cycles through three target devices (nRF_01, nRF_02, nRF_03)
- **Automatic reconnection**: Handles disconnections gracefully and moves to the next device
- **Data collection**: Collects non-zero data points from peripherals
- **Cloud publishing**: Publishes collected data to Particle Cloud's "state" event
- **Acknowledgment system**: Sends timestamps back to peripherals after receiving all data

## Data Structures

### DataPoint
Represents a single data measurement:
- `val1` (uint8_t): 8-bit value (0-255)
- `val2` (uint32_t): 32-bit value (0-4,294,967,295)
- `val3` (uint32_t): 32-bit value (0-4,294,967,295)
Total size: 9 bytes per point

### DataPacket
Container for transmitting data points:
- `packetNumber` (uint16_t): Current packet index
- `totalPackets` (uint16_t): Total number of packets in transmission
- `pointsInPacket` (uint8_t): Number of data points in this packet
- `points[]`: Array of DataPoint structures
Total size: 14 bytes for single-point packets

## Key Functions

### Initialization
- **`initBLE()`** (line 28): Initializes the BLE module
  - Turns on BLE
  - Sets device name to "Particle_Central_01"
  - Sets TX power to maximum (8) for better range
  - Registers disconnection callback

### Scanning and Connection
- **`startScanning()`** (line 59): Initiates BLE scanning for target devices
  - Uses blocking scan mode
  - Targets one device at a time in round-robin fashion
  - Processes scan results looking for current target

- **`processScanResult()`** (line 103): Evaluates found devices
  - Filters devices by name
  - Only connects to the current target device
  - Initiates connection if target is found

- **`connectToDevice()`** (line 154): Establishes BLE connection
  - Connects to peripheral
  - Resets data collection
  - Waits for connection parameter negotiation
  - Automatically discovers services

### Service Discovery
- **`discoverServices()`** (line 216): Discovers BLE services and characteristics
  - Finds custom service (UUID: 12345678-1234-1234-1234-123456789abc)
  - Locates data characteristic for notifications
  - Locates ACK characteristic for acknowledgments
  - Enables notifications on data characteristic

- **`enableNotifications()`** (line 320): Subscribes to data notifications
  - Checks characteristic properties
  - Sets up notification callback
  - Handles subscription errors with retry logic

### Data Handling
- **`onDataReceived()`** (line 371): Processes incoming data packets
  - Validates packet size (14-20 bytes)
  - Parses DataPacket structure
  - Collects non-zero data points
  - Tracks packet sequence
  - Triggers completion actions on last packet

- **`resetDataCollection()`** (line 511): Clears data buffers for new device

- **`publishCollectedData()`** (line 517): Publishes to Particle Cloud
  - Creates JSON-formatted event data
  - Includes device name, count, and data points
  - Publishes as "state" event

### Acknowledgment System
- **`sendAckWithTimestamp()`** (line 275): Sends acknowledgment to peripheral
  - Gets current Unix timestamp
  - Writes 4-byte timestamp to ACK characteristic
  - Includes timeout protection to prevent hanging

### Disconnection Handling
- **`onDisconnected()`** (line 462): Callback for disconnection events
  - Resets connection state
  - Advances to next target device
  - Logs disconnection details

- **`forceDisconnect()`** (line 483): Forces disconnection
  - Used when ACK sending might hang
  - Ensures clean state reset
  - Prevents connection state corruption

## Operation Flow

1. **Initialization**: BLE module starts, sets up as central device
2. **Scanning**: Scans for first target device (nRF_01)
3. **Connection**: Connects when target is found
4. **Service Discovery**: Discovers custom service and characteristics
5. **Data Reception**: Receives data packets via notifications
6. **Data Collection**: Accumulates non-zero data points
7. **Completion**: After receiving all packets:
   - Publishes data to cloud
   - Sends ACK with timestamp
   - Disconnects from device
8. **Round-Robin**: Moves to next device (nRF_02), repeats process

## Key Configuration

### UUIDs (Must match peripheral)
- Service: `12345678-1234-1234-1234-123456789abc`
- Data Characteristic: `87654321-4321-4321-4321-cba987654321`
- ACK Characteristic: `11223344-5566-7788-99aa-bbccddeeff00`

### Target Devices
- nRF_01
- nRF_02
- nRF_03

### Connection Parameters
- TX Power: 8 (maximum)
- Supervision timeout: Extended (negotiated by peripheral)
- Packet size: 14-20 bytes per notification

## Error Handling
- Connection failures trigger move to next device
- ACK send failures are handled with timeout protection
- Service discovery failures cause disconnection
- Packet validation ensures data integrity

## Cloud Integration
Data is published to Particle Cloud with format:
```json
{
  "device": "nRF_01",
  "count": 25,
  "data": "1:100:200,2:150:250,..."
}
```

## Debugging Features
- Extensive logging for all operations
- Packet timeout detection (5-second threshold)
- Connection state tracking
- Detailed error messages with context