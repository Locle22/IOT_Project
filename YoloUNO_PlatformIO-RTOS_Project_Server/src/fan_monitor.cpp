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
 * FanControlTask: Kiểm tra semaphore mỗi 1s để quyết định bật/tắt
 * ─────────────────────────────────────────────────────────────────
 *  Manual Override (từ Web): ưu tiên tuyệt đối
 *  Auto mode:  Bật nếu semTempWarning hoặc semTempCritical active
 *              Tắt nếu semTempNormal active
 */
void FanControlTask(void *pvParameters)
{
    FanInit();

    while (1) {
        if (fanManualOverride) {
            // ── Chế độ Manual (từ Web Server) ────────────────────────────────
            if (fanManualState) {
                FanON();
            } else {
                FanOFF();
            }
        } else {
            // ── Chế độ Auto (theo semaphore nhiệt độ) ────────────────────────
            bool isHot = (xSemaphoreTake(semTempWarning,  0) == pdTRUE);
            bool isCritical = (xSemaphoreTake(semTempCritical, 0) == pdTRUE);

            if (isHot || isCritical) {
                FanON();
                // Trả semaphore lại để task khác (LED, LCD) cũng đọc được
                if (isHot)      xSemaphoreGive(semTempWarning);
                if (isCritical) xSemaphoreGive(semTempCritical);
            } else {
                FanOFF();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
