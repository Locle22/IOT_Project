#include "LCDTask.h"

LiquidCrystal_I2C lcd(0x21, 16, 2);

void initLCD() {
    lcd.begin();
    lcd.backlight();  
    lcd.clear();
    lcd.setCursor(0, 0);
}

void TaskLCD(void *pvParameters) {
    const char* tempMsg = "";
    const char* humMsg = "";
    double temp = 0, hum = 0;
    
    while(1) {
        // ========== NHIỆT ĐỘ ==========
        if (xSemaphoreTake(semTempCold, pdMS_TO_TICKS(10)) == pdTRUE) {
            tempMsg = "T:COLD    ";
        } 
        else if (xSemaphoreTake(semTempNormal, pdMS_TO_TICKS(10)) == pdTRUE) {
            tempMsg = "T:NORMAL  ";
        } 
        else if (xSemaphoreTake(semTempElevated, pdMS_TO_TICKS(10)) == pdTRUE) {
            tempMsg = "T:ELEVATED";
        } 
        else if (xSemaphoreTake(semTempCritical, pdMS_TO_TICKS(10)) == pdTRUE) {
            tempMsg = "T:CRITICAL";
        }

        // ========== ĐỘ ẨM ==========
        if (xSemaphoreTake(semHumDry, pdMS_TO_TICKS(10)) == pdTRUE) {
            humMsg = "H:DRY     ";
        } 
        else if (xSemaphoreTake(semHumOptimal, pdMS_TO_TICKS(10)) == pdTRUE) {
            humMsg = "H:OPTIMAL ";
        } 
        else if (xSemaphoreTake(semHumHumid, pdMS_TO_TICKS(10)) == pdTRUE) {
            humMsg = "H:HUMID   ";
        } 
        else if (xSemaphoreTake(semHumExtreme, pdMS_TO_TICKS(10)) == pdTRUE) {
            humMsg = "H:EXTREME ";
        }

        // Đọc giá trị hiện tại để hiển thị số
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            dht20.read();
            temp = dht20.getTemperature();
            hum = dht20.getHumidity();
            xSemaphoreGive(i2cMutex);
        }

        // Cập nhật LCD
        if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
            lcd.clear();
            
            // Mức cảnh báo nhiệt độ
            lcd.setCursor(0, 0);
            lcd.print(tempMsg);
            lcd.setCursor(13, 0);
            lcd.print((int)temp);
            lcd.print("C");
            
            // Mức cảnh báo độ ẩm
            lcd.setCursor(0, 1);
            lcd.print(humMsg);
            lcd.setCursor(13, 1);
            lcd.print((int)hum);
            lcd.print("%");
            
            xSemaphoreGive(i2cMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
