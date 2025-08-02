#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H


#include <RTClib.h>
extern volatile int32_t start_time = 0;
extern volatile int32_t rtc_start_value = 0;  // Store the RTC value when we started

unsigned long toUnixTimestamp(int year, int month, int day, int hour, int minute, int second) {
  DateTime dt(year, month, day, hour, minute, second);
  return dt.unixtime();
}

int32_t calculateElapsed(int32_t start, int32_t end) {
  if (end >= start) {
    return end - start;
  } else {
    // Handle 24-bit overflow
    return (0xFFFFFF - start) + end + 1;
  }
}

void initRTC() {
  // Start LF clock first
  if (NRF_CLOCK->LFCLKSTAT == 0) {
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0);
    Serial.println("LF Clock started");
  }

  // Stop RTC2 completely
  NRF_RTC2->TASKS_STOP = 1;
  delay(10);

  // Clear the counter
  NRF_RTC2->TASKS_CLEAR = 1;
  delay(10);

  // Set prescaler while stopped
  NRF_RTC2->PRESCALER = 4095;

  // Verify prescaler was set
  Serial.print("Prescaler set to: ");
  Serial.println(NRF_RTC2->PRESCALER);

  // Clear events
  NRF_RTC2->EVENTS_TICK = 0;
  NRF_RTC2->EVENTS_OVRFLW = 0;
}

void resetRTC() {
  Serial.println("=== RESETTING COUNTER ===");
  NRF_RTC2->TASKS_STOP = 1;
  delay(10);
  NRF_RTC2->TASKS_CLEAR = 1;
  delay(10);
  start_time = 0;
  rtc_start_value = 0;
  NRF_RTC2->TASKS_START = 1;
  Serial.println("Counter reset to 0");
}

void startRTC() {
  // Start RTC2
  NRF_RTC2->TASKS_START = 1;
  rtc_start_value = NRF_RTC2->COUNTER;  // Remember where we started
  Serial.println("RTC2 has started at 8 Hz");
}

int32_t readRTC() {
  // Calculate elapsed time since last reset
  int32_t current_rtc = NRF_RTC2->COUNTER;
  int32_t elapsed_ticks = calculateElapsed(rtc_start_value, current_rtc);
  return elapsed_ticks;  // Convert to seconds and return
}

#endif // RTC_CLOCK_H