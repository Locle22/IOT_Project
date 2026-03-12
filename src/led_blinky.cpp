#include "led_blinky.h"

// Reference external semaphores from main.cpp
extern SemaphoreHandle_t semTempCold;
extern SemaphoreHandle_t semTempNormal;
extern SemaphoreHandle_t semTempElevated;
extern SemaphoreHandle_t semTempCritical;

void led_blinky(void *pvParameters) {
    pinMode(LED_GPIO, OUTPUT);
    int currentState = 1;       // Default to Normal
    uint8_t stepCounter = 0;    // Local counter for timing logic

    while(1) {
        int nextState = currentState;

        // Check Semaphores with 0 timeout for maximum responsiveness
        if (xSemaphoreTake(semTempCold, 0) == pdTRUE) {
            nextState = 0;
        } else if (xSemaphoreTake(semTempNormal, 0) == pdTRUE) {
            nextState = 1;
        } else if (xSemaphoreTake(semTempElevated, 0) == pdTRUE) {
            nextState = 2;
        } else if (xSemaphoreTake(semTempCritical, 0) == pdTRUE) {
            nextState = 3;
        }

        // Reset step counter if state changed
        if (nextState != currentState) {
            currentState = nextState;
            stepCounter = 0;
            digitalWrite(LED_GPIO, LOW); // Ensure LED starts in a known state on state change
        }

        // State machine using a single base delay
        stepCounter++;

        switch(currentState) {
            case 0: // Cold (<18°C): Slow blink (On 2s, Off 2s)
                if (stepCounter >= 20) {
                    digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
                    stepCounter = 0;
                }
                break;

            case 1: // Normal (18-28°C): Steady blink (On 500ms, Off 500ms)
                if (stepCounter >= 5) {
                    digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
                    stepCounter = 0;
                }
                break;

            case 2: // Elevated (28-35°C): Rapid blink (On 100ms, Off 100ms)
                digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
                stepCounter = 0;
                break;

            case 3: // Critical (>=35°C): Non-blocking SOS pattern
                // Total SOS cycle: 3*S(4ticks) + 5 ticks + 3*O(8ticks) + 5 ticks + 3*S(4ticks) + Gap(20ticks) = 78 ticks
                if (stepCounter < 17) { // 3 Short: ON 2, OFF 2
                    if (stepCounter < 12) {
                        digitalWrite(LED_GPIO, (stepCounter % 4 < 2) ? HIGH : LOW);
                    } else {
                        digitalWrite(LED_GPIO, LOW); // Gap 5 ticks
                    }
                } 
                else if (stepCounter < 46) { // 3 Long: ON 6, OFF 2 (offset 17)
                    uint32_t subStep = stepCounter - 17;
                    if (subStep < 24) {
                        digitalWrite(LED_GPIO, (subStep % 8 < 6) ? HIGH : LOW);
                    } else {
                        digitalWrite(LED_GPIO, LOW); // Gap 5 ticks
                    }
                } 
                else if (stepCounter < 78) { // 3 Short: ON 2, OFF 2
                    uint32_t subStep = stepCounter - 46;
                    if (subStep < 12) {
                        digitalWrite(LED_GPIO, (subStep % 4 < 2) ? HIGH : LOW);
                    } else {
                        digitalWrite(LED_GPIO, LOW); // Gap 20 ticks
                    }
                }

                // Reset SOS cycle
                if (stepCounter >= 77) { 
                    stepCounter = 0; 
                }
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}