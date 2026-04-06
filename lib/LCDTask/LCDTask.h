#ifndef LCD_TASK_H
#define LCD_TASK_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "SensorTask.h"

extern LiquidCrystal_I2C lcd;

void initLCD();
void TaskLCD(void *pvParameters);

#endif
