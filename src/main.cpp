#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <SPI.h>

#include "config.h"
#include "ThereminEngine.h"
#include "DisplayController.h"

// ========================================================
// ======= ESP-NOW 配置 ================================
// ========================================================

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#if ENABLE_ESPNOW
typedef struct __attribute__((packed)) struct_message {
    int a;
    int b;
    int c;
} struct_message;

struct_message sharedData;
volatile bool newDataReady = false;
volatile bool espNowTaskRunning = false;
TaskHandle_t espNowTaskHandle = NULL;
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
static unsigned long lastEspNowSend = 0;

void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {}

bool setupESPNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) return false;
    esp_now_register_send_cb(onDataSent);
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) return false;
    return true;
}

void sendESPNowData(int looking, int duty, int direction) {
    struct_message tempData = {looking, duty, direction};
    
    portENTER_CRITICAL(&dataMux);
    memcpy(&sharedData, &tempData, sizeof(struct_message));
    newDataReady = true;
    portEXIT_CRITICAL(&dataMux);
}

void espNowTask(void* pvParameters) {
    espNowTaskRunning = true;
    struct_message localData = {0, 0, 0};
    int lastA = -1, lastB = -1;
    
    while (espNowTaskRunning) {
        if (newDataReady) {
            portENTER_CRITICAL(&dataMux);
            memcpy(&localData, &sharedData, sizeof(struct_message));
            newDataReady = false;
            portEXIT_CRITICAL(&dataMux);
            
            // 10ms 节流
            if (millis() - lastEspNowSend > 10) {
                if (localData.a != lastA || localData.b != lastB) {
                    esp_now_send(broadcastAddress, (uint8_t*)&localData, sizeof(localData));
                    lastA = localData.a;
                    lastB = localData.b;
                    lastEspNowSend = millis();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelete(NULL);
}
#endif

// ========================================================
// ======= 全局对象 ================================
// ========================================================

ThereminEngine engine;
DisplayController display;

// ========================================================
// ======= 函数声明 ================================
// ========================================================

void blinkAnimation();

// ========================================================
// ======= 眨眼动画 ================================
// ========================================================

void blinkAnimation() {
    int style = random(0, 3);
    if (style == 0) {
        display.displayEyes(DisplayController::EYE_PARTIAL, DisplayController::EYE_PARTIAL);
        delay(100);
        display.displayEyes(DisplayController::EYE_CLOSED, DisplayController::EYE_CLOSED);
        delay(50);
        display.displayEyes(DisplayController::EYE_PARTIAL, DisplayController::EYE_PARTIAL);
    }
}

// ========================================================
// ======= 主函数 ================================
// ========================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    
    randomSeed(esp_random());
    
    Serial.println("=== ESP32 Theremin v3.4 ===");
    
    if (!display.begin()) Serial.println("ERROR: Display failed");
    if (!engine.begin()) Serial.println("ERROR: Engine failed");
    
    #if ENABLE_ESPNOW
    if (!setupESPNow()) {
        Serial.println("ERROR: ESP-NOW failed");
    } else {
        xTaskCreatePinnedToCore(espNowTask, "ESPNowTask", 2048, NULL, 2, &espNowTaskHandle, 1);
    }
    #endif
    
    Serial.println("System Initialized");
}

void loop() {
    engine.process();
    
    int currentLooking = engine.getLooking();
    display.updateEyes(currentLooking);
    
    // 调试打印
    static int lastPrint = 0;
    if (millis() - lastPrint > 500) {
        Serial.print("Looking: ");
        Serial.println(currentLooking);
        lastPrint = millis();
    }
    
    // 随机眨眼 - 500ms冷却，5%概率
    static unsigned long lastBlinkTime = 0;
    if (currentLooking == 0) {
        if (millis() - lastBlinkTime > 500) {
            if (random(1, 20) == 1) {
                Serial.println("BLINK!");
                blinkAnimation();
                lastBlinkTime = millis();
            }
        }
    }
    
    #if ENABLE_ESPNOW
    sendESPNowData(currentLooking, engine.getDuty(), engine.getDirection());
    #endif
}
