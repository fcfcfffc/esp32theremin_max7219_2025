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
// ESP-NOW数据结构
typedef struct __attribute__((packed)) struct_message {
    int a;  // looking值 (0-8)
    int b;  // PWM占空比 (0-255)
    int c;  // 方向值 (-1,0,1)
} struct_message;

volatile struct_message sharedData;
volatile bool newDataReady = false;
volatile bool espNowTaskRunning = false;
TaskHandle_t espNowTaskHandle = NULL;
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

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
    struct_message tempData;
    tempData.a = looking;
    tempData.b = duty;
    tempData.c = direction;
    
    portENTER_CRITICAL(&dataMux);
    sharedData.a = tempData.a;
    sharedData.b = tempData.b;
    sharedData.c = tempData.c;
    newDataReady = true;
    portEXIT_CRITICAL(&dataMux);
}

// 优化后的ESP-NOW任务 - 加快发送速度
void espNowTask(void* pvParameters) {
    espNowTaskRunning = true;
    struct_message localData = {0, 0, 0};
    int lastA = -1, lastB = -1;
    
    while (espNowTaskRunning) {
        if (newDataReady) {
            portENTER_CRITICAL(&dataMux);
            localData.a = sharedData.a;
            localData.b = sharedData.b;
            localData.c = sharedData.c;
            newDataReady = false;
            portEXIT_CRITICAL(&dataMux);
            
            // 仅在数据变化时发送，移除时间节流限制
            if (localData.a != lastA || localData.b != lastB) {
                esp_now_send(broadcastAddress, (uint8_t*)&localData, sizeof(localData));
                lastA = localData.a;
                lastB = localData.b;
            }
        }
        // 减少延迟到1ms，最高可达1000Hz发送频率
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
// ======= 主函数 ================================
// ========================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("=== ESP32 Theremin v3.3 (Optimized) ===");
    
    // 初始化显示
    if (!display.begin()) {
        Serial.println("ERROR: Display setup failed");
    }
    
    // 初始化引擎
    if (!engine.begin()) {
        Serial.println("ERROR: Engine setup failed");
    }
    
    // 初始化ESP-NOW
    #if ENABLE_ESPNOW
    if (!setupESPNow()) {
        Serial.println("ERROR: ESP-NOW setup failed");
    } else {
        xTaskCreatePinnedToCore(espNowTask, "ESPNowTask", 2048, NULL, 2, &espNowTaskHandle, 1);
    }
    #endif
    
    Serial.println("ESP32 Theremin Initialized");
}

void loop() {
    // 处理核心逻辑
    engine.process();
    
    // 更新显示
    display.updateEyes(engine.getLooking());
    display.processBlink();
    // 发送ESP-NOW数据
    #if ENABLE_ESPNOW
    sendESPNowData(engine.getLooking(), engine.getDuty(), engine.getDirection());
    #endif
}
