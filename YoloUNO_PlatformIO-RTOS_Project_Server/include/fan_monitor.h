#ifndef __FAN_MONITOR_H__
#define __FAN_MONITOR_H__

#include <Arduino.h>
#include "global.h"

// ─── Pin & PWM Config ────────────────────────────────────────────────────────
#define FAN_PIN         6
#define PWM_FREQ        25000
#define PWM_CHANNEL     0
#define PWM_RESOLUTION  8

void FanInit();
void FanON();
void FanOFF();
void FanSetSpeed(uint8_t speed);
uint8_t FanGetSpeed();

void FanSetManualOverride(bool enabled, bool manualState);
void FanClearManualOverride();
bool FanIsManualOverrideEnabled();
bool FanGetState();

void FanControlTask(void *pvParameters);

#endif
