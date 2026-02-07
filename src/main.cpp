#include <Arduino.h>
#include "driver/pcnt.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "LedControl.h"
#include <SPI.h>

// ======= 引脚定义 =======
#define PCNT_INPUT_SIG_IO   18
#define BUTTON_PIN          4
#define PWM_OUT_PIN         2
#define PCNT_UNIT           PCNT_UNIT_0

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
LedControl lc = LedControl(17, 15, 16, 8); // LedControl(DIN, CLK, CS, 8个模块)

// ======= 数据结构 =======
typedef struct __attribute__((packed)) struct_message {
    int a; // 对应 looking
    int b; // 对应 PWM duty 或其他同步值
} struct_message;

struct_message myData;

// ======= 全局变量 =======

// ========== 可调参数区 ==========

// 1. 稳定性判定参数
const int stableThreshold = 10;              // 稳定计数阈值（5-20），10次×20ms=0.2秒
const float freqChangeMax = 4.5;             // 频率变化最大允许值（3.0-8.0），超过此值判定为不稳定
const float rawDeltaReset = 7.5;             // 手靠近Δf阈值（6.0-8.0），超过此值重置稳定状态
const int stableLockCount = 5;               // 锁定计数阈值（3-10），防止Δf在临界点卡住

// 2. 自适应频率滤波参数
const float minAlphaFreq = 0.10;             // 频率稳定时的滤波系数（0.05-0.20），越小越平滑
const float maxAlphaFreq = 0.40;             // 频率突变时的滤波系数（0.30-0.55），越大响应越快
const float freqDiffDivisor = 50.0;          // 频率差分界点（30.0-75.0），达到此值时alpha为最大值

// 3. Δf自适应滤波参数
const float minAlphaDelta = 0.25;            // Δf稳定时的滤波系数（0.15-0.35）
const float maxAlphaDelta = 0.75;            // Δf突变时的滤波系数（0.60-0.85）
const float deltaDiffDivisor = 20.0;         // Δf变化分界点（15.0-30.0）

// 4. 映射范围参数
const float DELTA_MIN = 1.0;                 // Δf最小值（0.5-2.0），对应PWM=0, looking=0
const float DELTA_MAX = 8.0;                 // Δf最大值（6.0-10.0），对应PWM=255, looking=8

// ========== 功能开关 ==========
bool autoSetBase = true;                     // 自动基准更新开关（true=自动，false=手动）
bool enableESPNow = true;                    // ESP-NOW广播开关（true=开启，false=关闭）
bool enableSerial = true;                    // 串口监视开关（true=开启，false=关闭）

// ========== 运行时变量 ==========
int looking;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile int pulseCount = 0;
bool buttonPressed = false;
float smoothedFreq = 0.0;
float smoothedBaseFreq = 0.0;
float smoothedDelta = 0.0;
int baseFreq = 0;
bool baseFreqSet = false;
float lastSmoothedFreq = 0.0;
int stableCount = 0;
bool alreadySetBase = false;                 // alreadySetBase保护机制

// ======= LED点阵图案 =======
byte eyeopen[8] = {
    B00111100,B01000010,B10000001,B10011001,B10011001,B10000001,B01000010,B00111100
};
byte eyeclosed[8] = {
    B00011000,B00011000,B00011000,B00011000,B00011000,B00011000,B00011000,B00000000
};
byte eyepartial[8] = {
    B00000000,B00111100,B00100100,B00110100,B00110100,B00100100,B00111100,B00000000
};
byte eyepartialopen[8] = {
    B00000000,B00111100,B01000010,B01011010,B01011010,B01000010,B00111100,B00000000
};
byte slightleft[8] = {
    B00111100,B01000010,B10011001,B10011001,B10000001,B10000001,B01000010,B00111100
};
byte left[8] = {
    B00111100,B01011010,B10011001,B10000001,B10000001,B10000001,B01000010,B00111100
};
byte realleft[8] = {
    B00111100,B01011010,B10000001,B10000001,B10000001,B01000010,B00111100,B00000000
};
byte slightright[8] = {
    B00111100,B01000010,B10000001,B10000001,B10011001,B10011001,B01000010,B00111100
};
byte right[8] = {
    B00111100,B01000010,B10000001,B10000001,B10000001,B10011001,B01011010,B00111100
};
byte realright[8] = {
    B00000000,B01111110,B10000001,B10000001,B10000001,B10000001,B01011010,B00111100
};

// ======= 中断 =======
void IRAM_ATTR onTimer() {
    int16_t count;
    pcnt_get_counter_value(PCNT_UNIT, &count);
    portENTER_CRITICAL_ISR(&timerMux);
    pulseCount = count;
    portEXIT_CRITICAL_ISR(&timerMux);
    pcnt_counter_clear(PCNT_UNIT);
}

void IRAM_ATTR onButton() {
    buttonPressed = true;
}

// ======= 初始化 =======
void setupPCNT() {
    pcnt_config_t pcnt_config;
    pcnt_config.pulse_gpio_num = PCNT_INPUT_SIG_IO;
    pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    pcnt_config.unit = PCNT_UNIT;
    pcnt_config.channel = PCNT_CHANNEL_0;
    pcnt_config.pos_mode = PCNT_COUNT_INC;
    pcnt_config.neg_mode = PCNT_COUNT_DIS;
    pcnt_config.lctrl_mode = PCNT_MODE_KEEP;
    pcnt_config.hctrl_mode = PCNT_MODE_KEEP;
    pcnt_config.counter_h_lim = 32767;
    pcnt_config.counter_l_lim = -32767;

    pcnt_unit_config(&pcnt_config);
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    pcnt_counter_resume(PCNT_UNIT);
}

void setupTimer() {
    timer = timerBegin(1000000); // 1 MHz
    timerAttachInterrupt(timer, &onTimer);
    timerAlarm(timer, 50000, true, 0); // 200 ms
}

void setupPWM() {
    ledcAttach(PWM_OUT_PIN, 1000, 8);
    ledcWrite(PWM_OUT_PIN, 0);
}

void setupButton() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(BUTTON_PIN, onButton, FALLING);
}

void setupESPNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_send_cb([](const wifi_tx_info_t *info, esp_now_send_status_t status){});

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

// ======= 显示 =======
void displayEyes(byte leftEye[], byte rightEye[]) {
    for(int i=0;i<4;i++)
        for(int row=0;row<8;row++) lc.setRow(i,row,leftEye[row]);
    for(int i=4;i<8;i++)
        for(int row=0;row<8;row++) lc.setRow(i,row,rightEye[row]);
}

void blinkAnimation() {
    int style=random(0,3);
    if(style==0){
        displayEyes(eyepartial, eyepartial);
        delay(100);
        displayEyes(eyeclosed, eyeclosed);
        delay(50);
        displayEyes(eyepartial, eyepartial);
    }
}

// ======= Setup =======
void setup() {
    Serial.begin(115200);
    delay(100);
    for(int i=0;i<8;i++){
        lc.shutdown(i,false);
        lc.setIntensity(i,8);
        lc.clearDisplay(i);
    }
    setupPCNT();
    setupTimer();
    setupPWM();
    setupButton();
    setupESPNow();
    Serial.println("✅ ESP32-S3 Theremin 发送端已启动");
}

// ======= Loop =======
void loop() {
    delay(20);

    int currentFreq;
    portENTER_CRITICAL(&timerMux);
    currentFreq = pulseCount;
    portEXIT_CRITICAL(&timerMux);

    // ===== 自适应频率滤波 =====
    float freqDiff = fabs(currentFreq - smoothedFreq);
    float alphaFreq = constrain(minAlphaFreq + (freqDiff / freqDiffDivisor), minAlphaFreq, maxAlphaFreq);
    smoothedFreq = alphaFreq * currentFreq + (1 - alphaFreq) * smoothedFreq;

    // ===== 稳定性判定 =====
    // 含义：连续多次频率变化很小 → 认为手停住了

    float freqChange = fabs(smoothedFreq - lastSmoothedFreq);

    // 使用"当前 raw Δf"作为动作判断依据（不用 smoothedDelta）
    float rawDelta = smoothedBaseFreq - smoothedFreq;
    if (rawDelta < 0) rawDelta = 0;

    // 添加锁定计数器：当频率稳定时逐步累积，防止临界点跳动
    static int currentLockCount = 0;

    // 频率变化太大 → 清零所有状态
    if (freqChange >= freqChangeMax) {
        stableCount = 0;
        alreadySetBase = false;
        currentLockCount = 0;
    }
    // 手明显靠近（Δf ≥ rawDeltaReset）→ 重置稳定
    else if (rawDelta >= rawDeltaReset) {
        stableCount = 0;
        alreadySetBase = false;
        currentLockCount = 0;
    }
    // 中等Δf（3Hz ≤ Δf < 7.5Hz）：使用锁定机制防止卡住
    else if (rawDelta >= 3.0) {
        // 如果已经有足够的锁定计数，允许累积
        if (currentLockCount >= stableLockCount && !alreadySetBase) {
            stableCount++;
        } else {
            // 频率稳定时累积锁定计数
            currentLockCount++;
        }
    }
    // Δf小（< 3Hz）：正常累积
    else {
        if (!alreadySetBase) {
            stableCount++;
            currentLockCount++;
        }
    }

    bool isStable = (stableCount >= stableThreshold);
    if (isStable) alreadySetBase = true;
    lastSmoothedFreq = smoothedFreq;

    // ===== 按钮设定基准频率 =====
    if (buttonPressed) {
        baseFreq = smoothedFreq;
        smoothedBaseFreq = smoothedFreq;
        baseFreqSet = true;
        buttonPressed = false;
        stableCount = 0;
    }

    // ===== 自动基准设定 =====
    if (isStable && autoSetBase) {
        smoothedBaseFreq = smoothedFreq;
        alreadySetBase = true;
    }

    // ===== Δf 自适应滤波 =====
    float delta = smoothedBaseFreq - smoothedFreq;
    if (delta < 0) delta = 0;
    float deltaDiff = fabs(delta - smoothedDelta);
    float alphaDelta = constrain(minAlphaDelta + (deltaDiff / deltaDiffDivisor), minAlphaDelta, maxAlphaDelta);
    smoothedDelta = alphaDelta * delta + (1 - alphaDelta) * smoothedDelta;

    // ===== 映射PWM =====
    int duty = map(smoothedDelta, DELTA_MIN, DELTA_MAX, 0, 255);
    duty = constrain(duty, 0, 255);
    ledcWrite(PWM_OUT_PIN, duty);

    // ===== 映射眼睛状态 =====
    looking = map(smoothedDelta, DELTA_MIN, DELTA_MAX, 0, 8);
    looking = constrain(looking, 0, 8);

    switch(looking){
        case 8: displayEyes(realleft, realleft); break;
        case 7: displayEyes(left, left); break;
        case 6: displayEyes(slightleft, slightleft); break;
        case 5: displayEyes(eyeopen, eyeopen); break;
        case 4: displayEyes(slightright, slightright); break;
        case 3: displayEyes(right, right); break;
        case 2: displayEyes(realright, realright); break;
        case 1: displayEyes(eyepartialopen, eyepartialopen); break;
        case 0: displayEyes(eyepartial, eyepartial);
            if(random(1,20)==1) blinkAnimation();
            break;
    }

    // ===== ESP-NOW 广播 =====
    if (enableESPNow) {
        myData.a = looking;
        myData.b = duty;
        esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));
    }

    // ===== 串口监视 =====
    if (enableSerial) {
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 100) {
            Serial.printf("Freq: %.1f | Base: %.1f | Δf: %.1f | PWM: %d | StableCnt: %d | a:%d | b:%d\n",
                          smoothedFreq, smoothedBaseFreq, smoothedDelta, duty, stableCount, myData.a, myData.b);
            lastPrint = millis();
        }
    }
}
