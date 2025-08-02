#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <Wire.h>

#define LSM6DSOX_ADDR  0x6A
#define INT1_PIN       D3  // A3 pin

extern volatile bool motionDetected = false;

void handleMotionInterrupt() {
  motionDetected = true;
  Serial.println("MOTION WAS DETECTED");
}

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(LSM6DSOX_ADDR);
  Wire.write(reg);
  Wire.write(value);


  Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(LSM6DSOX_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(LSM6DSOX_ADDR, 1);
  return Wire.read();
}

void setupLSM6DSOX() {
  pinMode(INT1_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INT1_PIN), handleMotionInterrupt, RISING);
  // Force D0 LOW if CS is connected there
  pinMode(D0, OUTPUT);
  digitalWrite(D0, HIGH);  // Force I2C mode
  delay(100);
  // Reset
  writeRegister(0x12, 0x01);
  delay(100);

    
  // Check WHO_AM_I (should be 0x6C)
  uint8_t whoami = readRegister(0x0F);
  Serial.print("WHO_AM_I: 0x");
  Serial.println(whoami, HEX);

  // Disable gyro (keep gyro ODR = 0)
  writeRegister(0x11, 0x00);

  // Set accelerometer to 1.6 Hz, low power, ±2g
  writeRegister(0x10, 0x11);  // CTRL1_XL = ODR_XL = 1.6Hz (0x01), FS_XL = ±2g

  // Enable interrupt generation
  writeRegister(0x58, 0x80);  // TAP_CFG2: INTERRUPTS_ENABLE = 1

  // Enable interrupt on XL (accel) activity on INT1
  writeRegister(0x0D, 0x02);  // INT1_CTRL: INT1_SINGLE_TAP = 1 (just a dummy example)

  // Enable wake-up detection
  writeRegister(0x5B, 0x20);  // WAKE_UP_DUR: WU_DUR = 0, SINGLE_DOUBLE_TAP = 0
  writeRegister(0x5C, 0x02);  // WAKE_UP_THS: threshold

  // Enable wake-up interrupt
  writeRegister(0x5E, 0x20);  // MD1_CFG: Wake-Up interrupt routed to INT1

  delay(100);
}

#endif // ACCELEROMETER_H