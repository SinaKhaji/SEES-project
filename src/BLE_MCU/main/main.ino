#include <Arduino.h>
#include "BLEConnector.h"
#include "esp_sleep.h"

#define SESSION_DURATION 300000000ULL 

hw_timer_t*   timer          = nullptr;
volatile bool sessionExpired = false;

void ARDUINO_ISR_ATTR onTimerExpired() {
    sessionExpired = true;
}

void TaskBLE(void* pv) {
    SensorCfg* cfg = (SensorCfg*)pv;
    for (;;) {
        if (sessionExpired) vTaskDelete(nullptr);
        if (cfg->doConnect && cfg->advertisedDevice) {
            xSemaphoreTake(bleMutex, portMAX_DELAY);
            bool ok = connectToServer(cfg);
            xSemaphoreGive(bleMutex);
            Serial.printf("BLE %s: %s\n", cfg->name, ok ? "connected" : "failed");
            if (ok) {
                cfg->doConnect = false;
                if (!c4.doConnect && !e6.doConnect) {
                    timerStart(timer);
                    Serial.println("Both connected — 5 min timer started.");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void endSession() {
    Serial.println("Session complete — disconnecting.");
    if (c4.client && c4.client->isConnected()) c4.client->disconnect();
    if (e6.client && e6.client->isConnected()) e6.client->disconnect();
    BLEDevice::getScan()->stop();
    BLEDevice::getScan()->clearResults();
    timerStop(timer);

    Serial.println("Entering deep sleep.");
    esp_sleep_enable_timer_wakeup(SESSION_DURATION);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
        
    delay(10000);
    
    initBLE();
    timer = timerBegin(1000000);
    timerAttachInterrupt(timer, &onTimerExpired);
    timerAlarm(timer, SESSION_DURATION, true, 0);
    xTaskCreatePinnedToCore(TaskBLE, "BLE_C4", 12000, &c4, 1, NULL, 0);
    xTaskCreatePinnedToCore(TaskBLE, "BLE_E6", 12000, &e6, 1, NULL, 1);
}

void loop() {
    if (sessionExpired) endSession();
    delay(10);
}