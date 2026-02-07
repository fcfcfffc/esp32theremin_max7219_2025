<<<<<<< HEAD
# ESP32-S3 Theremin - 稳定版

基于ESP32-S3的热敏眼（Theremin）项目，通过频率差检测实现眼睛跟踪。

## 硬件连接

| 引脚 | 功能 | 连接 |
|------|------|------|
| GPIO18 | 差频输入 | PCNT频率信号 |
| GPIO4  | 按钮输入 | 手动重置基准频率 |
| GPIO2  | PWM输出 | 输出到接收设备 |
| GPIO17 | MAX7219 DIN | LED数据线 |
| GPIO15 | MAX7219 CLK | LED时钟线 |
| GPIO16 | MAX7219 CS | LED片选线 |

## 工作原理

```
GPIO18 → PCNT计数器 → 原始频率 → 自适应滤波 → 平滑频率
                                          ↓
计算Δf = Base - Freq → 自适应Δf滤波 → PWM/LED控制
                       ↑
              稳定性判定 + 智能基准更新
```

## 核心算法

### 1. 自适应频率滤波（适度加强版）

```cpp
float freqDiff = fabs(currentFreq - smoothedFreq);
float alphaFreq = constrain(0.10 + (freqDiff / 50.0), 0.10, 0.40);
smoothedFreq = alphaFreq * currentFreq + (1 - alphaFreq) * smoothedFreq;
```

- 频率稳定时：α小（0.10），强滤波（90%历史数据）
- 频率突变时：α大（0.40），快速响应（60%历史数据）
- 分界点：50Hz时达到最大alpha

---

### 2. 滤波强度调整指南

如果需要调整滤波强度，修改以下参数：

```cpp
// 自适应频率滤波参数
float alphaFreq = constrain(minAlpha + (freqDiff / divisor), minAlpha, maxAlpha);
```

| 模式 | minAlpha | maxAlpha | divisor | 适用场景 |
|------|----------|----------|---------|----------|
| 保守模式 | 0.05 | 0.25 | 75.0 | 环境噪声很大 |
| 适度加强 | **0.10** | **0.40** | **50.0** | **默认配置** |
| 标准模式 | 0.15 | 0.55 | 30.0 | 环境较稳定 |
| 快速响应 | 0.20 | 0.70 | 20.0 | 需要最快响应 |

---

### 2. 稳定性判定（防止临界点卡住）

```cpp
float freqChange = fabs(smoothedFreq - lastSmoothedFreq);
float rawDelta = smoothedBaseFreq - smoothedFreq;
if (rawDelta < 0) rawDelta = 0;

// 频率变化太大 → 清零
if (freqChange >= 3.0) {
    stableCount = 0;
    alreadySetBase = false;
}
// 手明显靠近（≥7.5Hz）→ 重置
else if (rawDelta >= 7.5) {
    stableCount = 0;
    alreadySetBase = false;
}
// 中等Δf（3-7.5Hz）→ 锁定机制
else if (rawDelta >= 3.0) {
    if (stableLockCount >= 5 && !alreadySetBase) {
        stableCount++;
    } else {
        stableLockCount++;
    }
}
// Δf小（<3Hz）→ 正常累积
else {
    if (!alreadySetBase) {
        stableCount++;
        stableLockCount++;
    }
}
```

**工作流程**：
- `stableLockCount`累积到5后，允许`stableCount`累积
- 避免Δf在6-7Hz临界点卡住
- 频率变化≥3Hz或Δf≥7.5Hz时清零

---

### 3. 自动基准更新

```cpp
bool isStable = (stableCount >= stableThreshold);
if (isStable && autoSetBase) {
    smoothedBaseFreq = smoothedFreq;
    alreadySetBase = true;
}
```

- 满足3个条件：稳定计数≥10、启用自动设置、alreadySetBase=false
- 直接赋值（不使用平滑系数），快速跟随

---

### 4. Δf自适应滤波

```cpp
float delta = smoothedBaseFreq - smoothedFreq;
if (delta < 0) delta = 0;
float deltaDiff = fabs(delta - smoothedDelta);
float alphaDelta = constrain(0.25 + (deltaDiff / 20.0), 0.25, 0.75);
smoothedDelta = alphaDelta * delta + (1 - alphaDelta) * smoothedDelta;
```

- 变化大时：α大（0.75），快速响应
- 变化小时：α小（0.25），强平滑

---

### 5. PWM和LED映射

```cpp
int duty = map(smoothedDelta, DELTA_MIN, DELTA_MAX, 0, 255);
int looking = map(smoothedDelta, DELTA_MIN, DELTA_MAX, 0, 8);
```

- Δf范围：1-8Hz（可配置）
- PWM范围：0-255
- LED状态：0-8（向左看...向右看）

---

## 参数配置

### 📊 核心可调参数

#### 1. 稳定性判定参数

| 参数名 | 默认值 | 可调范围 | 说明 |
|--------|--------|----------|------|
| `stableThreshold` | 10 | 5-20 | 稳定计数阈值，达到此值后设置基准频率（10次×20ms=0.2秒）|
| `freqChangeMax` | 4.5 | 3.0-8.0 | 频率变化的最大允许值，超过此值判定为不稳定并清零稳定计数 |
| `stableLockCount` | 5 | 3-10 | 锁定计数阈值，用于防止Δf在临界点卡住 |

**调整建议**：
- 环境噪声大：提高`freqChangeMax`到5.0-6.0
- 需要更快稳定：降低`stableThreshold`到5-8
- 频繁卡住：提高`stableLockCount`到6-8

---

#### 2. 手靠近检测参数

| 参数名 | 默认值 | 可调范围 | 说明 |
|--------|--------|----------|------|
| `rawDeltaReset` | 7.5 | 6.0-8.0 | Δf阈值，超过此值判定为手靠近并重置稳定状态 |

**调整建议**：
- 需要更灵敏：降低到6.0-6.5
- 避免误判：提高到8.0

---

#### 3. 自适应频率滤波参数

| 参数名 | 默认值 | 可调范围 | 说明 |
|--------|--------|----------|------|
| `minAlphaFreq` | 0.10 | 0.05-0.20 | 频率稳定时的滤波系数（越小越平滑，响应越慢）|
| `maxAlphaFreq` | 0.40 | 0.30-0.55 | 频率突变时的滤波系数（越大响应越快，抖动越大）|
| `freqDiffDivisor` | 50.0 | 30.0-75.0 | 频率差分界点，达到此值时alpha为最大值 |

**调整建议**：
- 环境噪声大：`0.05-0.30-75.0`（保守模式）
- 标准模式：`0.15-0.55-30.0`（快速响应）
- 平衡模式：`0.10-0.40-50.0`（默认，适度加强）

---

#### 4. Δf自适应滤波参数

| 参数名 | 默认值 | 可调范围 | 说明 |
|--------|--------|----------|------|
| `minAlphaDelta` | 0.25 | 0.15-0.35 | Δf稳定时的滤波系数 |
| `maxAlphaDelta` | 0.75 | 0.60-0.85 | Δf突变时的滤波系数 |
| `deltaDiffDivisor` | 20.0 | 15.0-30.0 | Δf变化分界点 |

**调整建议**：
- PWM抖动大：降低`maxAlphaDelta`到0.60-0.70
- 响应太慢：提高`maxAlphaDelta`到0.80-0.85

---

#### 5. 映射范围参数

| 参数名 | 默认值 | 可调范围 | 说明 |
|--------|--------|----------|------|
| `DELTA_MIN` | 1.0 | 0.5-2.0 | Δf最小值（对应PWM=0, looking=0）|
| `DELTA_MAX` | 8.0 | 6.0-10.0 | Δf最大值（对应PWM=255, looking=8）|

**调整建议**：
- 读取范围小：`0.5-6.0`
- 读取范围大：`2.0-10.0`

---

#### 6. 功能开关

| 参数名 | 默认值 | 说明 |
|--------|--------|------|
| `autoSetBase` | true | 自动基准更新开关（true=自动，false=手动）|
| `enableESPNow` | true | ESP-NOW广播开关（true=开启，false=关闭）|
| `enableSerial` | true | 串口监视开关（true=开启，false=关闭）|

**功能开关使用**：

```cpp
// 关闭ESP-NOW广播（只运行本地PWM和LED）
bool enableESPNow = false;

// 关闭串口监视（提高性能）
bool enableSerial = false;

// 关闭自动基准更新（仅使用按钮手动设置）
bool autoSetBase = false;
```

---

### 📈 Looking和Duty映射表

| Δf (Hz) | looking | 眼睛状态 | duty (PWM) | 说明 |
|---------|---------|----------|------------|------|
| < DELTA_MIN | 0 | 睁眼 | 0 | 手离开 |
| DELTA_MIN | 1 | 半开 | ~32 | 极近 |
| DELTA_MIN+1 | 2 | 完全向右 | ~64 | 很近 |
| DELTA_MIN+2 | 3 | 向右 | ~96 | 近 |
| DELTA_MIN+3 | 4 | 轻微向右 | ~128 | 较近 |
| (DELTA_MIN+DELTA_MAX)/2 | 5 | 睁眼 | ~160 | 中间 |
| DELTA_MAX-3 | 6 | 轻微向左 | ~192 | 较远 |
| DELTA_MAX-2 | 7 | 向左 | ~224 | 远 |
| DELTA_MAX-1 | 8 | 完全向左 | 255 | 最远 |
| ≥ DELTA_MAX | 8 | 完全向左 | 255 | 最远 |

**说明**：
- looking范围：0-8（整数）
- duty范围：0-255
- Δf范围：DELTA_MIN到DELTA_MAX平滑插值

---

### 🔧 快速调整指南

#### 场景1：环境噪声大，频发误判
```cpp
const float freqChangeMax = 6.0;          // 从4.5提高到6.0
const float minAlphaFreq = 0.05;          // 加强滤波
const float maxAlphaFreq = 0.30;
const float freqDiffDivisor = 75.0;
```

#### 场景2：响应太慢
```cpp
const int stableThreshold = 5;            // 从10降到5
const float minAlphaFreq = 0.15;
const float maxAlphaFreq = 0.55;
const float freqDiffDivisor = 30.0;
```

#### 场景3：PWM抖动大
```cpp
const float minAlphaDelta = 0.35;         // 加强Δf滤波
const float maxAlphaDelta = 0.60;
```

#### 场景4：Δf卡住不动
```cpp
const int stableLockCount = 8;            // 从5提高到8
const float freqChangeMax = 5.0;          // 适当放宽
```

---

### 📝 参数配置示例代码

```cpp
// ========== 稳定性判定 ==========
const int stableThreshold = 10;           // 稳定计数阈值
const float freqChangeMax = 4.5;          // 频率变化最大允许值
const float rawDeltaReset = 7.5;          // 手靠近Δf阈值
const int stableLockCount = 5;            // 锁定计数阈值

// ========== 自适应频率滤波 ==========
const float minAlphaFreq = 0.10;          // 最小alpha
const float maxAlphaFreq = 0.40;          // 最大alpha
const float freqDiffDivisor = 50.0;       // 频率差分界点

// ========== Δf自适应滤波 ==========
const float minAlphaDelta = 0.25;         // Δf最小alpha
const float maxAlphaDelta = 0.75;         // Δf最大alpha
const float deltaDiffDivisor = 20.0;      // Δf差分界点

// ========== 映射范围 ==========
const float DELTA_MIN = 1.0;              // Δf最小值
const float DELTA_MAX = 8.0;              // Δf最大值

// ========== 功能开关 ==========
bool autoSetBase = true;                  // 自动基准更新
bool enableESPNow = true;                 // ESP-NOW广播
bool enableSerial = true;                 // 串口监视
```

## 串口监视输出

```
Freq: 27300.5 | Base: 27306.9 | Δf: 6.1 | PWM: 182 | StableCnt: 0 | a:5 | b:182
```

| 字段 | 含义 | 说明 |
|------|------|------|
| Freq | 平滑后频率 | Hz |
| Base | 基准频率 | Hz |
| Δf | 频率差 | Hz（1-8范围） |
| PWM | PWM输出 | 0-255 |
| StableCnt | 稳定计数 | 0-10+ |
| a | 眼睛状态 | 0-8 |
| b | PWM值 | 0-255 |

## 使用流程

1. 启动设备，等待2秒初始化
2. 自动学习基准频率（启动10秒后）
3. 观察Serial输出，确认Freq和Base接近
4. 如需手动调整基准，按下按钮（GPIO4）
5. 靠近/离开手，观察Δf和looking变化

## 常见问题

### ❌ Δf卡在6-7Hz不动

**原因**：临界点状态冲突

**解决方案**：使用`stableLockCount`锁定机制（已实现）

---

### ❌ Base跟随频率漂移

**原因**：手靠近时基准仍在更新

**解决方案**：Δf≥7.5Hz时清零稳定状态（已实现）

---

### ❌ 响应太慢

**原因**：`stableThreshold`太大

**解决方案**：
```cpp
const int stableThreshold = 5;  // 从10降到5
```

---

### ❌ 抖动大

**原因**：滤波太弱

**解决方案**：
```cpp
// 降低最大alpha
float alphaFreq = constrain(0.1 + (freqDiff / 50.0), 0.1, 0.4);
```

## 眼睛状态对应表

| Δf (Hz) | looking | 眼睛状态 | PWM |
|---------|---------|----------|-----|
| 0-1 | 0 | 睁眼 | 0 |
| 1-2 | 1 | 半开 | ~32 |
| 2-3 | 2 | 完全向右 | ~64 |
| 3-4 | 3 | 向右 | ~96 |
| 4-5 | 4 | 轻微向右 | ~128 |
| 5-6 | 5 | 睁眼 | ~160 |
| 6-7 | 6 | 轻微向左 | ~192 |
| 7-8 | 7 | 向左 | ~224 |
| 8+ | 8 | 完全向左 | 255 |

## 技术规格

| 项目 | 参数 |
|------|------|
| 平台 | ESP32-S3 DevKitM-1 |
| 采样频率 | 5Hz（200ms周期） |
| 读取范围 | Δf: 1-8Hz |
| PWM频率 | 1000Hz |
| PWM分辨率 | 8bit |
| ESP-NOW | 广播模式 |

## 版本历史

### v3.1 参数优化版
- ✅ 适度加强滤波（alpha: 0.15→0.10, 0.55→0.40）
- ✅ 提高频率变化阈值（3.0Hz→4.5Hz）
- ✅ 添加完整可调参数文档
- ✅ 添加功能开关（ESP-NOW、串口）
- ✅ 优化参数配置指南

### v3.0 稳定版
- ✅ 添加stableLockCount锁定机制
- ✅ 修复临界点卡住问题
- ✅ 优化Δf阈值（1-8Hz范围）
- ✅ 使用自适应滤波算法

---

**最后更新**: 2025-02-07
**版本**: v3.0（稳定版）
=======
ESP32 Frequency-to-PWM with ESP-NOW Transmission
This project implements a frequency detection and PWM generation system using an ESP32. It reads an input signal's frequency using the ESP32's Pulse Counter (PCNT) hardware, compares it with a base reference frequency, maps the frequency difference to a PWM duty cycle, and transmits the result wirelessly via ESP-NOW to a peer ESP32 device.

🧠 Key Features
Frequency Measurement: Uses hardware PCNT on pin GPIO 1 to measure input frequency (e.g., from a beat signal or oscillator).

PWM Output: Maps frequency difference to an 8-bit PWM signal on GPIO 2.

Wireless Communication: Transmits PWM values via ESP-NOW to another ESP32 (define peer MAC).

Base Frequency Calibration:

Manual: Press a button (GPIO 19) to set the base reference frequency.

Auto: When the signal is stable, the base frequency updates automatically.

Stability Detection: Uses smoothed frequency changes to determine signal stability.

📦 Hardware Requirements
ESP32 (e.g., ESP32-S3)

Signal input (e.g., sensor output or waveform generator)

Push button (connected to GPIO 19)

PWM-controllable device (e.g., LED or servo)

Another ESP32 as receiver (for ESP-NOW)

📶 Pin Configuration
Function	GPIO	Description
Frequency Input	1	Input signal for PCNT
Button Input	19	Manual base frequency set
PWM Output	2	Outputs PWM (0–255)

📋 License
MIT License
>>>>>>> c1f70764133a0778c4c36cfeb24ff696bc39affb
