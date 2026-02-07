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
const int stableThreshold = 10;
int stableCount = 0;
bool autoSetBase = true; // 自动基准设定

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
    float alphaFreq = constrain(0.15 + (freqDiff / 30.0), 0.15, 0.55);
    smoothedFreq = alphaFreq * currentFreq + (1 - alphaFreq) * smoothedFreq;

// ===== 稳定性判定（最小简化版） =====
// 含义：连续多次频率变化很小 → 认为手停住了

float freqChange = fabs(smoothedFreq - lastSmoothedFreq);

if (freqChange < 3.0) {          // 3Hz 内认为稳定（2~4 可调）
    stableCount++;
} else {
    stableCount = 0;
}

bool isStable = (stableCount >= stableThreshold);
lastSmoothedFreq = smoothedFreq;

    // ===== 按钮设定基准频率 =====
    if(buttonPressed){
        baseFreq = smoothedFreq;
        smoothedBaseFreq = smoothedFreq;
        baseFreqSet = true;
        buttonPressed=false;
        stableCount=0;
    }

    // ===== 自动基准设定 =====
    if(isStable && autoSetBase)
        smoothedBaseFreq = smoothedFreq;

    // ===== Δf 自适应滤波 =====
    float delta = smoothedBaseFreq - smoothedFreq;
    if(delta<0) delta=0;
    float deltaDiff = fabs(delta - smoothedDelta);
    float alphaDelta = constrain(0.25 + (deltaDiff/20.0), 0.25, 0.75);
    smoothedDelta = alphaDelta * delta + (1 - alphaDelta) * smoothedDelta;

    // ===== 映射PWM =====
    int duty = map(smoothedDelta,5,12,0,255);
    duty = constrain(duty,0,255);
    ledcWrite(PWM_OUT_PIN,duty);

    // ===== 映射眼睛状态 =====
    looking = map(smoothedDelta,5,12,0,8);
    looking = constrain(looking,0,8);

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
    myData.a = looking;
    myData.b = duty;
    esp_now_send(broadcastAddress,(uint8_t*)&myData,sizeof(myData));

    // ===== 串口监视 =====
    static unsigned long lastPrint=0;
    if(millis()-lastPrint>100){
        Serial.printf("Freq: %.1f | Base: %.1f | Δf: %.1f | PWM: %d | StableCnt: %d | a:%d | b:%d\n",
                      smoothedFreq, smoothedBaseFreq, smoothedDelta, duty, stableCount, myData.a, myData.b);
        lastPrint=millis();
    }
}
