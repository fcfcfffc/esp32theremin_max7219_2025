#include "ThereminEngine.h"
#include "config.h"

// 全局配置
ThereminConfig config;

// ========================================================
// ======= 中断处理 (ISRs) ===============================
// ========================================================

static ThereminEngine* s_engineInstance = nullptr;

void IRAM_ATTR ThereminEngine::onTimerISR() {
    if (s_engineInstance) {
        int count;
        pcnt_unit_get_count(s_engineInstance->m_pcntUnit, &count);
        pcnt_unit_clear_count(s_engineInstance->m_pcntUnit);
        
        portENTER_CRITICAL_ISR(&s_engineInstance->m_timerMux);
        s_engineInstance->m_pulseCount = count;
        s_engineInstance->m_dataReady = true;
        portEXIT_CRITICAL_ISR(&s_engineInstance->m_timerMux);
    }
}

void IRAM_ATTR ThereminEngine::onButtonISR() {
    if (s_engineInstance) {
        s_engineInstance->m_buttonPressed = true;
    }
}

// ========================================================
// ======= 构造函数与初始化 ==============================
// ========================================================

ThereminEngine::ThereminEngine() 
    : m_pcntUnit(nullptr)
    , m_pcntChannel(nullptr)
    , m_timer(nullptr)
    , m_pulseCount(0)
    , m_buttonPressed(false)
    , m_dataReady(false)
    , m_duty(0)
    , m_delta(0)
{
    m_timerMux = portMUX_INITIALIZER_UNLOCKED;
    m_baselineMux = portMUX_INITIALIZER_UNLOCKED;
}

bool ThereminEngine::begin() {
    // 设置ISR回调实例指针 (必须在其他初始化之前)
    s_engineInstance = this;
    
    // 初始化硬件
    if (!setupPCNT()) {
        Serial.println("ERROR: PCNT setup failed");
        return false;
    }
    
    if (!setupTimer()) {
        Serial.println("ERROR: Timer setup failed");
        return false;
    }
    
    if (!setupPWM()) {
        Serial.println("ERROR: PWM setup failed");
        return false;
    }
    
    setupButton();
    
    Serial.println("Theremin Engine Started");
    return true;
}

// ========================================================
// ======= 硬件初始化函数 ================================
// ========================================================

bool ThereminEngine::setupPCNT() {
    pinMode(config.pcntPin, INPUT);
    
    pcnt_unit_config_t uc = {.low_limit = -32767, .high_limit = 32767};
    if (pcnt_new_unit(&uc, &m_pcntUnit) != ESP_OK) return false;
    
    pcnt_glitch_filter_config_t gf = {.max_glitch_ns = 100};
    if (pcnt_unit_set_glitch_filter(m_pcntUnit, &gf) != ESP_OK) return false;
    
    pcnt_chan_config_t cc = {.edge_gpio_num = config.pcntPin, .level_gpio_num = -1};
    if (pcnt_new_channel(m_pcntUnit, &cc, &m_pcntChannel) != ESP_OK) return false;
    
    pcnt_channel_set_edge_action(m_pcntChannel,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    
    if (pcnt_unit_enable(m_pcntUnit) != ESP_OK) return false;
    pcnt_unit_clear_count(m_pcntUnit);
    pcnt_unit_start(m_pcntUnit);
    
    return true;
}

bool ThereminEngine::setupTimer() {
    m_timer = timerBegin(1000000);
    if (!m_timer) return false;
    timerAttachInterrupt(m_timer, &onTimerISR);
    timerAlarm(m_timer, config.samplingPeriodMs * 1000, true, 0);
    timerStart(m_timer);
    return true;
}

bool ThereminEngine::setupPWM() {
    ledcAttach(config.pwmPin, 1000, 8);
    ledcWrite(config.pwmPin, 0);
    return true;
}

void ThereminEngine::setupButton() {
    pinMode(config.buttonPin, INPUT_PULLUP);
    attachInterrupt(config.buttonPin, &onButtonISR, FALLING);
}

// ========================================================
// ======= 核心处理函数 ================================
// ========================================================

void ThereminEngine::process() {
    // ===== 频率采集 =====
    int currentFreq;
    portENTER_CRITICAL(&m_timerMux);
    if (m_dataReady) {
        currentFreq = m_pulseCount;
        m_dataReady = false;
    } else {
        portEXIT_CRITICAL(&m_timerMux);
        return;
    }
    portEXIT_CRITICAL(&m_timerMux);
    
    // ===== 频率滤波 =====
    freqState.smoothedFreq = filterFrequency(currentFreq, freqState.smoothedFreq);
    freqState.deltaRate = currentFreq - freqState.lastRawFreq;
    freqState.lastRawFreq = currentFreq;
    
    // ===== 基线初始化 =====
    initBaseline(freqState.smoothedFreq);
    
    // ===== 计算Delta =====
    float deltaRaw = freqState.frozenBaseFreq - freqState.smoothedFreq;
    stabState.direction = (deltaRaw > config.directionThreshold) ? -1 : 
                         (deltaRaw < -config.directionThreshold) ? 1 : 0;
    m_delta = fabs(deltaRaw);
    
    // ===== Delta滤波 =====
    freqState.lastSmoothedDelta = filterDelta(m_delta, freqState.lastSmoothedDelta);
    
    // ===== PWM输出 =====
    updatePWM();
    
    // ===== 稳定性判断 =====
    updateStability(m_delta);
    
    // ===== 环境噪音检测 =====
    detectEnvironmentJitter(freqState.deltaRate);
    
    // ===== 静态基线调整 =====
    updateStaticBaseline(m_delta, freqState.deltaRate);
    
    staticState.lastDeltaRaw = deltaRaw;
    
    // ===== 自适应基线更新 =====
    if (config.autoSetBase) {
        updateAdaptiveBaseline(m_delta, deltaRaw);
    }
    
    // ===== 手动校准 =====
    recalibrate();
    
    // ===== 眼睛映射 =====
    stabState.looking = map(freqState.lastSmoothedDelta * 10, 
                           config.deltaFMin * 10, config.deltaFMax * 10, 0, 8);
    stabState.looking = constrain(stabState.looking, 0, 8);
    // 输出平滑滤波 (EMA)
    float lookAlpha = 0.2f;  // 输出平滑系数
    stabState.smoothedLooking = lookAlpha * stabState.looking + (1 - lookAlpha) * stabState.smoothedLooking;
    
    // ===== 调试输出 =====
    debugOutput();
}

// 频率EMA滤波
float ThereminEngine::filterFrequency(float currentFreq, float smoothedFreq) {
    float diff = fabs(currentFreq - smoothedFreq);
    float alpha = diff > config.freqThresholdSpike ? config.alphaFreqSpike :
                  diff > config.freqThresholdMedium ? 
                      min(config.alphaFreqBase + diff * config.alphaFreqDynamic, config.alphaFreqMax) :
                      config.alphaFreqSmall;
    return alpha * currentFreq + (1 - alpha) * smoothedFreq;
}

// Delta EMA滤波
float ThereminEngine::filterDelta(float delta, float lastSmoothedDelta) {
    float alphaD = min(config.alphaDeltaBase + delta * config.alphaDeltaDynamic, config.alphaDeltaMax);
    return alphaD * delta + (1 - alphaD) * lastSmoothedDelta;
}

// 稳定性判断
void ThereminEngine::updateStability(float delta) {
    if (freqState.lastStableDelta == 0) {
        freqState.lastStableDelta = delta;
        return;
    }
    
    if (fabs(delta - freqState.lastStableDelta) <= config.stabilityThreshold) {
        freqState.stableCount++;
        if (freqState.stableCount >= config.stableWindow) {
            freqState.lastStableDelta = delta;
            freqState.stableCount = config.stableWindow;
        }
    } else {
        freqState.stableCount = 0;
        freqState.lastStableDelta = delta;
    }
}
// 环境噪音检测 (方案B改进)
// 核心思路：
// - 环境噪音 = 频率在0附近小幅抖动
// - 手移动 = 频率大幅单向变化
// 当 deltaRate 很大时（手移动），清除环境检测状态
void ThereminEngine::detectEnvironmentJitter(float deltaRate) {
    if (millis() - envState.lastSignCheck > config.envCheckInterval) {
        // 方案B改进：当 deltaRate 很大时（手移动），清除环境检测状态
        if (fabs(deltaRate) > config.envDeltaRateThreshold) {
            // 手在移动，清除噪音计数
            envState.envCount = 0;
        }
        
        // 正常的环境噪音检测（仅当 deltaRate 较小时）
        if (fabs(deltaRate) <= config.envDeltaRateThreshold) {
            if (envState.lastDeltaRateForEnv != 0 && deltaRate != 0) {
                bool signChanged = (envState.lastDeltaRateForEnv > 0 && deltaRate < 0) ||
                                   (envState.lastDeltaRateForEnv < 0 && deltaRate > 0);
                
                if (signChanged) {
                    envState.envCount = min(envState.envCount + 1, config.envWindow);
                } else {
                    envState.envCount = max(0, envState.envCount - 2);
                }
            }
        }
        
        envState.lastDeltaRateForEnv = deltaRate;
        envState.lastSignCheck = millis();
        
        bool currentEnv = (envState.envCount >= config.envCountThreshold);
        if (currentEnv) {
            envState.envStableCounter = min(envState.envStableCounter + 1, config.envStableWindow);
            envState.envClearCounter = 0;
        } else {
            envState.envClearCounter++;
            if (envState.envClearCounter >= config.envClearThreshold) {
                envState.envStableCounter = 0;
            }
        }
        envState.isEnvironmentalJitter = currentEnv;
    }
}

// 静态基线调整
void ThereminEngine::updateStaticBaseline(float delta, float deltaRate) {
    if (delta < config.staticDeltaThreshold) {
        float deltaDiff = fabs(deltaRate);
        
        if (deltaDiff < config.staticDeltaRateMax) {
            staticState.staticCount++;
        } else {
            staticState.staticCount = max(0, staticState.staticCount - config.staticPenalty);
        }
        
        if (staticState.staticCount > config.staticCountMax) {
            portENTER_CRITICAL(&m_baselineMux);
            freqState.smoothedBaseFreq = freqState.smoothedBaseFreq * 0.8f + freqState.smoothedFreq * 0.2f;
            freqState.frozenBaseFreq = freqState.smoothedFreq;
            portEXIT_CRITICAL(&m_baselineMux);
            staticState.staticCount = 0;
        }
    } else {
        // delta 较大，说明不是静态状态，重置计数器防止卡住
        staticState.staticCount = 0;
    }
}
// 三因子自适应基线更新 (方案B)
// 核心：始终允许基线缓慢跟随，让dR趋向于0
void ThereminEngine::updateAdaptiveBaseline(float delta, float deltaRaw) {
    float deltaAbs = delta;
    float baseAlpha = 0.05f + deltaAbs * 0.01f;
    float envFactor = envState.isEnvironmentalJitter ? config.envFactorValue : 0.0f;
    
    // handFactor 连续控制：二次曲线，小delta影响很小，大delta几乎完全锁死基线
    float handRatio = delta / config.handFactorThreshold;
    handRatio = handRatio * handRatio;  // 二次曲线
    float handFactor = (!envState.isEnvironmentalJitter) ?
                       baseAlpha * 0.98f * fminf(handRatio, 1.0f) : 0.0f;  // 98%抵消
    
    float adaptiveAlpha = baseAlpha + envFactor - handFactor;
    adaptiveAlpha = fmaxf(0.002f, fminf(0.5f, adaptiveAlpha));  // 恢复原范围

    // 缓存用于调试输出
    m_lastBaseAlpha = baseAlpha;
    m_lastEnvFactor = envFactor;
    m_lastHandFactor = handFactor;
    m_lastAdaptiveAlpha = adaptiveAlpha;
    
    // frozenBaseFreq 更新：稳定时快速跟随 + 无条件慢速漂移恢复（防死锁）
    if (delta <= 0.5f && freqState.stableCount >= config.stableWindow * 0.7f &&
        millis() - freqState.lastFrozenUpdate > config.frozenUpdateInterval) {
        freqState.frozenBaseFreq = freqState.smoothedFreq;
        freqState.lastFrozenUpdate = millis();
    } else {
        // 慢速漂移恢复：delta越大漂移越慢（手靠近时几乎不漂移）
        float driftAlpha = 0.002f / fmaxf(1.0f, delta);
        freqState.frozenBaseFreq = driftAlpha * freqState.smoothedBaseFreq +
                                   (1 - driftAlpha) * freqState.frozenBaseFreq;
    }

    portENTER_CRITICAL(&m_baselineMux);
    freqState.smoothedBaseFreq = adaptiveAlpha * freqState.smoothedFreq + 
                                 (1 - adaptiveAlpha) * freqState.smoothedBaseFreq;
    portEXIT_CRITICAL(&m_baselineMux);
}

// 基线初始化
void ThereminEngine::initBaseline(float smoothedFreq) {
    if (!freqState.baselineSet && smoothedFreq > 1000.0f) {
        if (initState.freqAtStartup == 0) {
            initState.freqAtStartup = smoothedFreq;
            initState.initCount = 0;
        }
        
        float freqDiff = fabs(smoothedFreq - initState.freqAtStartup);
        if (freqDiff < 5.0) {
            initState.initCount++;
            if (initState.initCount >= 10) {
                freqState.smoothedBaseFreq = smoothedFreq;
                freqState.frozenBaseFreq = smoothedFreq;
                freqState.baselineSet = true;
                Serial.printf("Baseline set to: %.1f\n", smoothedFreq);
            }
        } else {
            initState.freqAtStartup = smoothedFreq;
            initState.initCount = 0;
        }
    }
}

// PWM输出
void ThereminEngine::updatePWM() {
    m_duty = map(freqState.lastSmoothedDelta * 10, 
                config.deltaFMin * 10, config.deltaFMax * 10, 0, 255);
    m_duty = constrain(m_duty, 0, 255);
    ledcWrite(config.pwmPin, m_duty);
}

// 手动校准
void ThereminEngine::recalibrate() {
    bool pressed = false;
    portENTER_CRITICAL(&m_timerMux);
    pressed = m_buttonPressed;
    if (pressed) m_buttonPressed = false;
    portEXIT_CRITICAL(&m_timerMux);
    
    if (pressed) {
        portENTER_CRITICAL(&m_baselineMux);
        freqState.smoothedBaseFreq = freqState.smoothedFreq;
        freqState.frozenBaseFreq = freqState.smoothedFreq;
        portEXIT_CRITICAL(&m_baselineMux);
        freqState.stableCount = 0;
    }
}

// 调试输出
void ThereminEngine::debugOutput() {
#if DEBUG_MODE_ALPHA
    static unsigned long lastAlphaPrint = 0;
    if (millis() - lastAlphaPrint > 100) {
        float deltaRaw = freqState.frozenBaseFreq - freqState.smoothedFreq;
        Serial.printf("dR:%.2f es:%d sc:%d ba:%.3f ef:%.3f hf:%.3f a:%.4f\n",
                    deltaRaw, envState.envStableCounter, staticState.staticCount,
                    m_lastBaseAlpha, m_lastEnvFactor, m_lastHandFactor, m_lastAdaptiveAlpha);
        lastAlphaPrint = millis();
    }
#endif

#if DEBUG_MODE_PLOTTER
    static unsigned long lastPlotterPrint = 0;
    if (millis() - lastPlotterPrint > 50) {
        Serial.print(freqState.smoothedFreq);
        Serial.print(" ");
        Serial.print(freqState.smoothedBaseFreq);
        Serial.print(" ");
        Serial.print(m_delta);
        Serial.print(" ");
        Serial.println(freqState.deltaRate);
        lastPlotterPrint = millis();
    }
#endif

#if DEBUG_MODE_SIMPLE
    static unsigned long lastSimplePrint = 0;
    if (millis() - lastSimplePrint > 100) {
        Serial.printf("Freq: %.1f | Base: %.1f | d: %.2f | L: %d\n",
                    freqState.smoothedFreq, freqState.smoothedBaseFreq, m_delta, stabState.looking);
        lastSimplePrint = millis();
    }
#endif
}
