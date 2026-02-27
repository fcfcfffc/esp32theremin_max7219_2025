#ifndef THEREMIN_ENGINE_H
#define THEREMIN_ENGINE_H

#include <Arduino.h>
#include "driver/pulse_cnt.h"
#include "config.h"

// ========================================================
// ======= 状态结构体 (State Management) ===============
// ========================================================

// 频率处理状态
struct FrequencyState {
    float smoothedFreq = 0;         // 平滑后的当前频率
    float smoothedBaseFreq = 0;     // 平滑后的基线频率
    float lastSmoothedDelta = 0;   // 上一次平滑后的delta
    float lastStableDelta = 0;      // 上一次稳定的delta
    float lastRawFreq = 0;          // 上一次原始频率
    float deltaRate = 0;            // 频率变化率
    float frozenBaseFreq = 0;       // 冻结的基线频率
    unsigned long lastFrozenUpdate = 0;
    int stableCount = 0;            // 稳定计数器
    bool baselineSet = false;       // 基线是否已设置
};

// 眼睛和方向状态
struct EyeState {
    int looking = 0;                // 眼睛注视方向 (0-8)
    float smoothedLooking = 0;     // 平滑后的眼睛方向
    int direction = 0;             // 频率变化方向 (-1, 0, 1)
};

// 环境检测状态
struct EnvironmentState {
    bool isEnvironmentalJitter = false;
    float lastDeltaRateForEnv = 0;
    int envCount = 0;
    int envStableCounter = 0;
    int envClearCounter = 0;
    unsigned long lastSignCheck = 0;
};

// 静态调整状态
struct StaticAdjustState {
    float lastDeltaRaw = 0;
    uint16_t staticCount = 0;
};

// 基线初始化状态
struct InitState {
    float freqAtStartup = 0;
    int initCount = 0;
};

// ========================================================
// ======= ThereminEngine 类 ============================
// ========================================================

class ThereminEngine {
public:
    ThereminEngine();
    
    // 初始化
    bool begin();
    
    // 主循环处理
    void process();
    
    // 获取当前状态
    int getLooking() const { return (int)stabState.smoothedLooking; }
    int getDuty() const { return m_duty; }
    int getDirection() const { return stabState.direction; }
    float getDelta() const { return m_delta; }
    float getSmoothedFreq() const { return freqState.smoothedFreq; }
    float getSmoothedBaseFreq() const { return freqState.smoothedBaseFreq; }
    bool isBaselineSet() const { return freqState.baselineSet; }
    
    // 手动校准
    void recalibrate();
    
private:
    // 硬件初始化
    bool setupPCNT();
    bool setupTimer();
    bool setupPWM();
    void setupButton();
    
    // 频率处理
    float filterFrequency(float currentFreq, float smoothedFreq);
    float filterDelta(float delta, float lastSmoothedDelta);
    void updateStability(float delta);
    void detectEnvironmentJitter(float deltaRate);
    void updateStaticBaseline(float delta, float deltaRate);
    void updateAdaptiveBaseline(float delta, float deltaRaw);
    void initBaseline(float smoothedFreq);
    
    // 输出
    void updatePWM();
    
    // 调试输出
    void debugOutput();
    
    // 成员变量
    FrequencyState freqState;
    EyeState stabState;
    EnvironmentState envState;
    StaticAdjustState staticState;
    InitState initState;
    
    pcnt_unit_handle_t m_pcntUnit;
    pcnt_channel_handle_t m_pcntChannel;
    hw_timer_t* m_timer;
    
    volatile int m_pulseCount;
    volatile bool m_buttonPressed;
    volatile bool m_dataReady;
    
    int m_duty;
    float m_delta;

    // 调试缓存 (避免debugOutput重复计算)
    float m_lastBaseAlpha = 0;
    float m_lastEnvFactor = 0;
    float m_lastHandFactor = 0;
    float m_lastAdaptiveAlpha = 0;
    
    portMUX_TYPE m_timerMux;
    portMUX_TYPE m_baselineMux;
    
    // 中断处理
    static void IRAM_ATTR onTimerISR();
    static void IRAM_ATTR onButtonISR();
    friend void IRAM_ATTR onTimerISR();
    friend void IRAM_ATTR onButtonISR();
};

#endif // THEREMIN_ENGINE_H
