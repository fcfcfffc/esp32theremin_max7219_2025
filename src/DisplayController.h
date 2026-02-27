#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include <Arduino.h>
#include "LedControl.h"
#include "config.h"

// ========================================================
// ======= DisplayController 类 ==========================
// ========================================================

class DisplayController {
public:
    DisplayController();
    
    // 初始化
    bool begin();
    
    // 更新眼睛显示 (带脏标记，仅变化时刷新)
    void updateEyes(int looking);
    
    // 非阻塞眨眼处理 (每次loop调用)
    void processBlink();
    
    // 清理显示
    void clear();
    
private:
    LedControl* m_lc = nullptr;
    
    // 脏标记 - 仅在looking变化时刷新LED
    int m_lastLooking = -1;
    
    // 非阻塞眨眼状态机
    enum BlinkState { BLINK_IDLE, BLINK_PARTIAL, BLINK_CLOSED, BLINK_REOPEN };
    BlinkState m_blinkState = BLINK_IDLE;
    unsigned long m_blinkStartTime = 0;
    
    // 眼睛图案 (const, 存储在flash)
    static const byte EYE_OPEN[8];
    static const byte EYE_CLOSED[8];
    static const byte EYE_PARTIAL[8];
    static const byte EYE_PARTIAL_OPEN[8];
    static const byte EYE_SLIGHT_LEFT[8];
    static const byte EYE_LEFT[8];
    static const byte EYE_REAL_LEFT[8];
    static const byte EYE_SLIGHT_RIGHT[8];
    static const byte EYE_RIGHT[8];
    static const byte EYE_REAL_RIGHT[8];
    
    void displayEyes(const byte eyeL[], const byte eyeR[]);
};

#endif // DISPLAY_CONTROLLER_H
