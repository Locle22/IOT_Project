#include "fan_monitor.h"

// ─── Queue nhận lệnh (size=1, overwrite — luôn giữ lệnh mới nhất) ───────────
QueueHandle_t xQueueFanCmd = xQueueCreate(1, sizeof(FanCommand));

// ─── Trạng thái nội bộ task (chỉ FanControlTask đọc/ghi — KHÔNG chia sẻ) ────
static bool    s_fanOn      = false;
static uint8_t s_fanSpeed   = 255;
static bool    s_manualMode = false;
static bool    s_manualState = false;

// ─── Khởi tạo phần cứng ─────────────────────────────────────────────────────
void FanInit()
{
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(FAN_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);  // Tắt quạt khi khởi động
}

// ─── Getter (read-only, an toàn vì chỉ task duy nhất ghi) ───────────────────
bool    FanGetState() { return s_fanOn; }
uint8_t FanGetSpeed() { return s_fanSpeed; }

// ─── Helper nội bộ ──────────────────────────────────────────────────────────
static void fanApplyOn()
{
    ledcWrite(PWM_CHANNEL, s_fanSpeed);
    s_fanOn = true;
}

static void fanApplyOff()
{
    ledcWrite(PWM_CHANNEL, 0);
    s_fanOn = false;
}

/*
 * FanControlTask
 * ─────────────────────────────────────────────────────────────────────
 * Không dùng biến toàn cục — mọi lệnh đến qua xQueueFanCmd
 *
 *  FAN_CMD_MANUAL_ON   → Bật quạt, khóa auto
 *  FAN_CMD_MANUAL_OFF  → Tắt quạt, khóa auto
 *  FAN_CMD_AUTO        → Trả về chế độ tự động
 *  FAN_CMD_SET_SPEED   → Đặt tốc độ PWM (0-255), áp dụng ngay nếu đang bật
 *
 *  Auto mode: Bật khi temp >= 30°C, Tắt khi temp < 28°C (hysteresis 2°C)
 */
void FanControlTask(void *pvParameters)
{
    FanInit();
    SensorData sd = {0.0f, 0.0f};
    FanCommand cmd;

    while (1) {
        // ── Nhận lệnh mới từ queue (non-blocking) ───────────────────────
        if (xQueueReceive(xQueueFanCmd, &cmd, 0) == pdTRUE) {
            switch (cmd.cmd) {
                case FAN_CMD_MANUAL_ON:
                    s_manualMode  = true;
                    s_manualState = true;
                    fanApplyOn();
                    Serial.println("[Fan] MANUAL ON");
                    break;

                case FAN_CMD_MANUAL_OFF:
                    s_manualMode  = true;
                    s_manualState = false;
                    fanApplyOff();
                    Serial.println("[Fan] MANUAL OFF");
                    break;

                case FAN_CMD_AUTO:
                    s_manualMode = false;
                    Serial.println("[Fan] → AUTO mode");
                    break;

                case FAN_CMD_SET_SPEED:
                    s_fanSpeed = cmd.speed;
                    if (s_fanOn) {
                        ledcWrite(PWM_CHANNEL, s_fanSpeed);  // Áp dụng ngay
                    }
                    Serial.printf("[Fan] Speed → %d\n", s_fanSpeed);
                    break;
            }
        }

        // ── Xử lý logic ─────────────────────────────────────────────────
        if (s_manualMode) {
            // Manual: giữ nguyên trạng thái đã set
            if (s_manualState && !s_fanOn)  fanApplyOn();
            if (!s_manualState && s_fanOn)  fanApplyOff();
        } else {
            // Auto: dựa trên nhiệt độ từ Queue
            if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
                if (sd.temp >= 30.0f && !s_fanOn) {
                    fanApplyOn();
                    Serial.printf("[Fan] AUTO ON (T=%.1f >= 30)\n", sd.temp);
                }
                else if (sd.temp < 28.0f && s_fanOn) {
                    fanApplyOff();
                    Serial.printf("[Fan] AUTO OFF (T=%.1f < 28)\n", sd.temp);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
