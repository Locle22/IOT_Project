#include "fan_monitor.h"

static const uint8_t FAN_SPEED_STOP = 0;

// ─── State (local, không phải biến toàn cục) ─────────────────────────────────
static volatile bool    fanManualOverride = false;
static volatile bool    fanManualState    = false;
static volatile bool    fanCurrentState   = false;
static volatile uint8_t fanSpeed          = 255;

// ─── Khởi tạo ────────────────────────────────────────────────────────────────
void FanInit()
{
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(FAN_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, FAN_SPEED_STOP);  // Tắt quạt khi khởi động
}

// ─── Điều khiển cơ bản ───────────────────────────────────────────────────────
void FanON()
{
    ledcWrite(PWM_CHANNEL, fanSpeed);
    fanCurrentState = true;
}

void FanOFF()
{
    ledcWrite(PWM_CHANNEL, FAN_SPEED_STOP);
    fanCurrentState = false;
}

void FanSetSpeed(uint8_t speed)
{
    fanSpeed = speed;
    if (fanCurrentState) {
        ledcWrite(PWM_CHANNEL, fanSpeed);  // Áp dụng ngay nếu đang chạy
    }
}

uint8_t FanGetSpeed()  { return fanSpeed; }
bool    FanGetState()  { return fanCurrentState; }

// ─── Manual Override (ưu tiên cao hơn auto) ──────────────────────────────────
void FanSetManualOverride(bool enabled, bool manualState)
{
    fanManualOverride = enabled;
    fanManualState    = manualState;
}

void FanClearManualOverride()
{
    fanManualOverride = false;
}

bool FanIsManualOverrideEnabled() { return fanManualOverride; }

/*
 * FanControlTask: Đọc nhiệt độ từ Queue để quyết định bật/tắt
 * ─────────────────────────────────────────────────────────────
 *  Manual Override (từ Web): ưu tiên tuyệt đối
 *  Auto mode:  Bật nếu temp >= 30°C
 *              Tắt nếu temp < 30°C (hysteresis: tắt khi < 28°C tránh nhấp nháy)
 */
void FanControlTask(void *pvParameters)
{
    FanInit();

    SensorData sd = {0.0f, 0.0f};

    while (1) {
        if (fanManualOverride) {
            // ── Chế độ Manual (từ Web Server) ────────────────────────────────
            if (fanManualState) {
                FanON();
            } else {
                FanOFF();
            }
        } else {
            // ── Chế độ Auto (theo nhiệt độ từ Queue) ─────────────────────────
            if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                if (sd.temp >= 30.0f) {
                    if (!fanCurrentState) {
                        FanON();
                        Serial.printf("[Fan] AUTO ON (T=%.1f >= 30)\n", sd.temp);
                    }
                } else if (sd.temp < 28.0f) {
                    // Hysteresis: chỉ tắt khi < 28°C tránh bật/tắt liên tục quanh 30°C
                    if (fanCurrentState) {
                        FanOFF();
                        Serial.printf("[Fan] AUTO OFF (T=%.1f < 28)\n", sd.temp);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
