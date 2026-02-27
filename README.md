# ESP32 Theremin Eye Display

![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![Version](https://img.shields.io/badge/version-v3.5-orange)
![License](https://img.shields.io/badge/license-MIT-green)

基于 ESP32-S3 的电容式频率检测系统，通过 8 个级联 MAX7219 LED 矩阵显示"表情眼睛"。采用三因子自适应基线算法，支持环境噪音自动识别和冻结基线防死锁机制。

---

## 版本信息

**当前版本**: v3.5 (2026年2月)
**发布日期**: 2026年2月
**推荐用途**: 实时手势控制、互动艺术装置

---

## 功能特性

### 核心功能
- **高频采样**: 20ms 采样周期 (50Hz)
- **三因子自适应基线**: baseAlpha + envFactor - handFactor 动态调整
- **连续 handFactor**: 二次曲线控制，无硬阈值跳变，98% 抵消
- **环境噪音检测**: deltaRate 符号变化频率区分环境抖动与手移动
- **双基线系统**: smoothedBaseFreq (持续跟随) + frozenBaseFreq (条件更新 + 防死锁漂移)
- **非阻塞眨眼**: 状态机驱动，不阻塞主循环
- **脏标志渲染**: 仅在 looking 值变化时刷新 LED，减少 SPI 开销
- **输出平滑滤波**: EMA 平滑眼睛状态 (α=0.2)
- **ESP-NOW 广播**: Core 1 独立任务发送频率数据 (已优化至1ms延迟)
- **PWM 输出**: 1kHz 频率 8 位精度信号

### v3.5 更新
- ✅ 重构为模块化代码结构：ThereminEngine、DisplayController、config
- ✅ 眨眼逻辑：500ms冷却 + 5%概率 + 1/3执行率
- ✅ ESP-NOW：10ms节流，防止发送过快

### v3.4 更新
- ✅ 修复 looking=0 时眨眼不工作的问题
- ✅ 优化 ESP-NOW 发送速度：任务延迟从 10ms 降至 1ms
- ✅ 移除 50ms 发送节流，数据变化立即发送
- ✅ 移除 200ms 超时空发，仅变化时发送

---

## 硬件连接

| 功能 | GPIO | 描述 |
|------|------|------|
| 频率输入 | 18 | 外部差频信号 (PCNT 双边沿检测) |
| PWM输出 | 2 | PWM 信号输出 (1kHz, 8-bit) |
| 校准按钮 | 4 | 手动基线校准 (INPUT_PULLUP) |
| LED矩阵-DIN | 17 | 数据输入 (MAX7219) |
| LED矩阵-CLK | 15 | 时钟 (MAX7219) |
| LED矩阵-CS | 16 | 片选 (MAX7219) |

### 硬件清单

| 组件 | 数量 | 说明 |
|------|------|------|
| ESP32-S3 开发板 | 1 | 主控制器 |
| MAX7219 LED 点阵 | 8 | 8x8 点阵模块级联 |
| 差频信号源 | 1 | 连接到 PCNT 输入 |
| 按钮 | 1 | 手动校准按钮 |

---

## 软件架构

```
src/
├── main.cpp              # 主入口、ESP-NOW任务(Core1)、主循环
├── config.h              # 引脚定义 + 算法参数 + ThereminConfig结构体
├── ThereminEngine.h      # 5个状态结构体 + 引擎类声明
├── ThereminEngine.cpp    # 核心算法：采样→滤波→基线→delta→映射
├── DisplayController.h   # 显示类 + 眨眼状态机枚举
└── DisplayController.cpp # 10种static const眼睛图案、脏标志渲染
```

### 数据流

```
Timer ISR (20ms)
  └─ PCNT读取脉冲计数
      └─ process()
          ├─ 频率EMA滤波 (自适应α)
          ├─ 基线初始化 (开机自动)
          ├─ deltaRaw = frozenBase - smoothedFreq
          ├─ delta EMA滤波
          ├─ PWM输出
          ├─ 稳定性判断 (stableWindow)
          ├─ 环境噪音检测 (deltaRate符号变化)
          ├─ 静态基线调整
          ├─ 三因子自适应基线更新
          │   ├─ smoothedBaseFreq ← adaptiveAlpha × EMA
          │   └─ frozenBaseFreq ← 条件快速更新 / 慢速防死锁漂移
          └─ 眼睛映射 + 输出平滑
              └─ DisplayController
                  ├─ 脏标志检测 → SPI刷新8模块
                  └─ 非阻塞眨眼状态机
```

---

## 核心算法

### 1. 三因子自适应基线

```cpp
float baseAlpha = 0.05f + delta * 0.01f;           // 基础因子
float envFactor = isEnvironmentalJitter ? 0.2 : 0;  // 环境因子

// handFactor 连续控制（二次曲线，无硬阈值跳变）
float handRatio = delta / handFactorThreshold;       // 归一化
handRatio = handRatio * handRatio;                   // 二次曲线
float handFactor = baseAlpha * 0.98 * min(handRatio, 1.0);  // 98%抵消

float adaptiveAlpha = baseAlpha + envFactor - handFactor;
adaptiveAlpha = clamp(0.002, 0.5);
```

### 2. 冻结基线防死锁

```cpp
// 稳定时快速更新
if (delta <= 0.5 && stableCount >= 70% && 间隔 > 3s)
    frozenBaseFreq = smoothedFreq;

// 不稳定时慢速漂移恢复（防死锁）
else
    driftAlpha = 0.002 / max(1.0, delta);  // delta越大漂移越慢
    frozenBaseFreq = driftAlpha * smoothedBaseFreq + (1-driftAlpha) * frozenBaseFreq;
```

---

## 编译与上传

```bash
# 使用 PlatformIO
pio run --target upload

# 或使用 Arduino IDE
# 1. 安装 ESP32 板支持
# 2. 安装 LedControl 库
# 3. 编译并上传
```

### 串口监视器

```bash
pio device monitor -b 115200
```

---

## 眼睛表情映射

| Δf 范围 (Hz) | looking 值 | 表情 |
|--------------|------------|------|
| 0-4 | 0-1 | 闭合/部分闭合 (含眨眼) |
| 4-7 | 2-3 | 右看 |
| 7-9 | 4-5 | 中间/右偏 |
| 9-12 | 6-8 | 左看 |

---

## 调试模式

编辑 `config.h` 切换：

```cpp
#define DEBUG_MODE_ALPHA    true   // alpha 系数调试 (推荐)
#define DEBUG_MODE_PLOTTER  false  // 串口绘图器模式
#define DEBUG_MODE_SIMPLE   false  // 简单调试模式
```

### Alpha 调试输出格式

```
dR:-0.72 es:0 sc:1 ba:0.057 ef:0.000 hf:0.003 a:0.0546
```

| 字段 | 说明 |
|------|------|
| dR | deltaRaw = frozenBaseFreq - smoothedFreq |
| es | envStableCounter (环境噪音持续计数) |
| sc | staticCount (静态基线调整计数) |
| ba | baseAlpha (基础因子) |
| ef | envFactor (环境因子，0 或 0.2) |
| hf | handFactor (手因子，连续值) |
| a | adaptiveAlpha (最终自适应系数) |

---

## 版本历史

### v3.5 (2026年2月)
- ✅ 重构为模块化架构：ThereminEngine / DisplayController / config
- ✅ 眨眼逻辑优化：500ms冷却 + 5%概率 + 1/3执行率
- ✅ ESP-NOW 10ms发送节流
- ✅ 原始眨眼动画恢复

### v3.4 (2026年2月)
- ✅ 修复 looking=0 时眨眼不工作的问题
- ✅ 优化 ESP-NOW 发送速度：任务延迟 10ms → 1ms
- ✅ 移除 50ms 发送节流限制
- ✅ 移除 200ms 超时空发

### v3.3 (2026年2月)
- ✅ handFactor 改为连续二次曲线控制，消除 3.0Hz 阈值跳变
- ✅ frozenBaseFreq 防死锁漂移恢复 (driftAlpha = 0.002/delta)
- ✅ handFactor 抵消比例 80% → 98%
- ✅ 非阻塞眨眼状态机
- ✅ 脏标志渲染

### v3.2 (2025年2月)
- ✅ 重构为模块化代码结构
- ✅ 10ms 采样周期 (100Hz)
- ✅ 三因子自适应基线算法

---

## 许可证

MIT License - see [LICENSE](LICENSE) file for details.

---

## 致谢

- [LedControl](https://github.com/wayoda/LedControl) - MAX7219 驱动库
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) - ESP32 支持
