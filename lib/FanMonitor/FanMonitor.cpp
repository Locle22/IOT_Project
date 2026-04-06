#include "FanMonitor.h"
#include "SensorTask.h"

const int FAN_PIN = 6;           
const int PWM_FREQ = 25000;       
const int PWM_CHANNEL = 0;        
const int PWM_RESOLUTION = 8;
const uint8_t FAN_SPEED_STOP = 0;

static volatile bool fanManualOverride = false;
static volatile bool fanManualState = false;
static volatile bool fanCurrentState = false;
static volatile uint8_t fanSpeed = 255;

void FanInit() {
    // 1. Cấu hình thông số cho kênh PWM
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    
    // 2. Gắn kênh PWM vào chân GPIO 6
    ledcAttachPin(FAN_PIN, PWM_CHANNEL);
    
    // 3. Đảm bảo quạt tắt lúc mới khởi động mạch
    ledcWrite(PWM_CHANNEL, FAN_SPEED_STOP);
}

void FanON() {   
    ledcWrite(PWM_CHANNEL, fanSpeed);
    fanCurrentState = true;
}

void FanOFF() {
    ledcWrite(PWM_CHANNEL, FAN_SPEED_STOP);
    fanCurrentState = false;
}

void FanSetSpeed(uint8_t speed) {
    fanSpeed = speed;

    // Nếu quạt đang bật thì áp dụng tốc độ mới ngay.
    if (fanCurrentState) {
        ledcWrite(PWM_CHANNEL, fanSpeed);
    }
}

uint8_t FanGetSpeed() {
    return fanSpeed;
}

void FanSetManualOverride(bool enabled, bool manualState) {
    fanManualOverride = enabled;
    fanManualState = manualState;
}

void FanClearManualOverride() {
    fanManualOverride = false;
}

bool FanIsManualOverrideEnabled() {
    return fanManualOverride;
}

bool FanGetState() {
    return fanCurrentState;
}

void FanControlTask(void *pvParameters) {
    while(1) {
        // Manual override (web) có ưu tiên cao hơn logic nhiệt độ.
        if (fanManualOverride) {
            if (fanManualState) {
                FanON();
            } else {
                FanOFF();
            }
        } else {
            // Auto mode: bật quạt khi nhiệt độ ELEVATED hoặc CRITICAL.
            bool isElevated = (xSemaphoreTake(semTempNormal, 0) == pdTRUE);
            bool isCritical = (xSemaphoreTake(semTempCritical, 0) == pdTRUE);

            if (isElevated || isCritical) {
                FanON();
            } else {
                FanOFF();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Kiểm tra lại sau 1 giây
    }
}