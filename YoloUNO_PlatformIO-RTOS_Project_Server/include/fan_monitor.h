#ifndef __FAN_MONITOR_H__
#define __FAN_MONITOR_H__

#include <Arduino.h>
#include "global.h"

// ─── Pin & PWM Config ────────────────────────────────────────────────────────
#define FAN_PIN         6
#define PWM_FREQ        25000
#define PWM_CHANNEL     0
#define PWM_RESOLUTION  8

// ─── Fan Command (truyền qua Queue, thay thế biến toàn cục) ─────────────────
typedef enum {
    FAN_CMD_AUTO,       // Chế độ tự động theo nhiệt độ
    FAN_CMD_MANUAL_ON,  // Bật thủ công
    FAN_CMD_MANUAL_OFF, // Tắt thủ công
    FAN_CMD_SET_SPEED   // Đặt tốc độ PWM
} FanCmdType;

typedef struct {
    FanCmdType cmd;
    uint8_t    speed;   // Chỉ dùng khi cmd == FAN_CMD_SET_SPEED
} FanCommand;

// Queue nhận lệnh từ WebServer / CoreIOT → FanControlTask
extern QueueHandle_t xQueueFanCmd;

void FanInit();
bool FanGetState();
uint8_t FanGetSpeed();

void FanControlTask(void *pvParameters);

#endif
