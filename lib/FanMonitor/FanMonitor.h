#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <Arduino.h>

void FanON();
void FanOFF();
void FanInit();
void FanControlTask(void *pvParameters);

// Manual override from web: when enabled, fan follows manualState.
void FanSetManualOverride(bool enabled, bool manualState);
void FanClearManualOverride();
bool FanIsManualOverrideEnabled();
bool FanGetState();
void FanSetSpeed(uint8_t speed);
uint8_t FanGetSpeed();

#endif // FAN_CONTROL_H