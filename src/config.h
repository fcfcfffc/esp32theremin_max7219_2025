#ifndef CONFIG_H
#define CONFIG_H

// ========================================================
// ======= 硬件引脚定义 (Hardware Pins) =================
// ========================================================
#define PCNT_INPUT_SIG_IO   18  // 频率输入引脚 (PCNT计数器)
#define BUTTON_PIN          4   // 基线校准按钮 (INPUT_PULLUP)
#define PWM_OUT_PIN         2   // PWM输出引脚 (1kHz, 8-bit)
#define LED_DIN_PIN         17  // LED矩阵数据引脚 (MAX7219 DIN)
#define LED_CLK_PIN         15  // LED矩阵时钟引脚 (MAX7219 CLK)
#define LED_CS_PIN          16  // LED矩阵片选引脚 (MAX7219 CS)
#define LED_MODULE_COUNT    8   // LED矩阵模块数量 (8x8点阵)

// ========================================================
// ======= 算法参数 (Algorithm Parameters) ===============
// ========================================================

// 采样与稳定
#define SAMPLING_PERIOD_MS  20  // 采样周期 (毫秒) - 10ms = 40Hz采样
#define STABLE_WINDOW       20  // 稳定窗口大小 (采样次数)50 //20
#define STABILITY_THRESHOLD 0.2f  // 稳定性判断阈值 (Hz)
#define DIRECTION_THRESHOLD 0.2f // 方向判断阈值

// 频率映射
#define DELTA_F_MIN         4.0 // 最小频率差值 (Hz)
#define DELTA_F_MAX         12.0 // 最大频率差值 (Hz)

// 频率滤波系数 (自适应EMA)
#define FREQ_THRESHOLD_SPIKE    50.0f   // 尖峰频率阈值 (Hz)
#define FREQ_THRESHOLD_MEDIUM  5.0f    // 中等频率阈值 (Hz)
#define ALPHA_FREQ_SPIKE       0.05f   // 尖峰滤波系数
#define ALPHA_FREQ_SMALL       0.08f   // 小变化滤波系数
#define ALPHA_FREQ_BASE        0.10f   // 基础滤波系数
#define ALPHA_FREQ_DYNAMIC    0.005f  // 动态滤波系数
#define ALPHA_FREQ_MAX        0.35f   // 最大频率滤波系数

// Delta滤波系数
#define ALPHA_DELTA_BASE       0.5f
#define ALPHA_DELTA_DYNAMIC    0.05f
#define ALPHA_DELTA_MAX        0.7f

// ========================================================
// ======= 环境噪音检测参数 ==============================
// ========================================================
#define ENV_WINDOW            10    // 环境检测窗口大小
#define ENV_STABLE_WINDOW     200   // 环境稳定窗口最大值
#define ENV_COUNT_THRESHOLD   2     // 环境噪音判断阈值
#define ENV_CHECK_INTERVAL    20    // 环境检测间隔 (毫秒)
#define ENV_CLEAR_THRESHOLD   1     // envStableCounter清零阈值
#define ENV_DELTA_RATE_THRESHOLD 15.0f // 判定手移动的deltaRate阈值

// ========================================================
// ======= 静态基线检测参数 ==============================
// ========================================================
#define STATIC_DELTA_THRESHOLD 1.5f  // 静态状态delta阈值 (改为1.5，只有稳定时<1.5才累加)
#define STATIC_DELTA_RATE_MAX  20.0f // 最大频率变化率 (提高)
#define STATIC_COUNT_MAX       10    // 触发阈值 (从20降到10)
#define STATIC_PENALTY         10     // 惩罚值

// ========================================================
// ======= 自适应基线因子参数 ============================
// ========================================================
#define ENV_FACTOR_VALUE       0.2f   // 环境噪音因子值
#define HAND_FACTOR_THRESHOLD 3.0f   // 手动因子阈值 (从1.0提高到3.0，只有较大变化才触发)
#define HAND_FACTOR_COEFF      0.03f  // 手动因子系数
#define FROZEN_UPDATE_INTERVAL 3000  // frozenBaseFreq更新间隔 (毫秒)

// ========================================================
// ======= 功能开关 (Feature Flags) ======================
// ========================================================
#define ENABLE_ESPNOW       true
#define AUTO_SET_BASE       true
#define DEBUG_MODE_PLOTTER  false   // 串口绘图器模式
#define DEBUG_MODE_SIMPLE   false   // 简单调试模式
#define DEBUG_MODE_ALPHA    false    // alpha系数调试模式

// ========================================================
// ======= 配置结构体 (Runtime Configuration) ============
// ========================================================

struct ThereminConfig {
    // 硬件
    int pcntPin = PCNT_INPUT_SIG_IO;
    int buttonPin = BUTTON_PIN;
    int pwmPin = PWM_OUT_PIN;
    int ledDinPin = LED_DIN_PIN;
    int ledClkPin = LED_CLK_PIN;
    int ledCsPin = LED_CS_PIN;
    int ledModuleCount = LED_MODULE_COUNT;
    
    // 算法
    int samplingPeriodMs = SAMPLING_PERIOD_MS;
    int stableWindow = STABLE_WINDOW;
    float deltaFMin = DELTA_F_MIN;
    float deltaFMax = DELTA_F_MAX;
    float stabilityThreshold = STABILITY_THRESHOLD;
    float directionThreshold = DIRECTION_THRESHOLD;
    
    // 滤波
    float freqThresholdSpike = FREQ_THRESHOLD_SPIKE;
    float freqThresholdMedium = FREQ_THRESHOLD_MEDIUM;
    float alphaFreqSpike = ALPHA_FREQ_SPIKE;
    float alphaFreqSmall = ALPHA_FREQ_SMALL;
    float alphaFreqBase = ALPHA_FREQ_BASE;
    float alphaFreqDynamic = ALPHA_FREQ_DYNAMIC;
    float alphaFreqMax = ALPHA_FREQ_MAX;
    float alphaDeltaBase = ALPHA_DELTA_BASE;
    float alphaDeltaDynamic = ALPHA_DELTA_DYNAMIC;
    float alphaDeltaMax = ALPHA_DELTA_MAX;
    
    // 环境检测
    int envWindow = ENV_WINDOW;
    int envStableWindow = ENV_STABLE_WINDOW;
    int envCountThreshold = ENV_COUNT_THRESHOLD;
    int envCheckInterval = ENV_CHECK_INTERVAL;
    int envClearThreshold = ENV_CLEAR_THRESHOLD;
    float envDeltaRateThreshold = ENV_DELTA_RATE_THRESHOLD;
    
    // 静态调整
    float staticDeltaThreshold = STATIC_DELTA_THRESHOLD;
    float staticDeltaRateMax = STATIC_DELTA_RATE_MAX;
    int staticCountMax = STATIC_COUNT_MAX;
    int staticPenalty = STATIC_PENALTY;
    
    // 自适应因子
    float envFactorValue = ENV_FACTOR_VALUE;
    float handFactorThreshold = HAND_FACTOR_THRESHOLD;
    float handFactorCoeff = HAND_FACTOR_COEFF;
    int frozenUpdateInterval = FROZEN_UPDATE_INTERVAL;
    
    // 功能开关
    bool enableEspNow = ENABLE_ESPNOW;
    bool autoSetBase = AUTO_SET_BASE;
    bool debugModePlotter = DEBUG_MODE_PLOTTER;
    bool debugModeSimple = DEBUG_MODE_SIMPLE;
    bool debugModeAlpha = DEBUG_MODE_ALPHA;
};

// 全局配置实例
extern ThereminConfig config;

#endif // CONFIG_H
