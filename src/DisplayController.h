#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include <Arduino.h>
#include "LedControl.h"
#include "config.h"

class DisplayController {
public:
    DisplayController();
    
    bool begin();
    
    // 基本显示更新 (脏标记)
    void updateEyes(int looking);
    
    // 强制刷新 (用于眨眼动画后恢复图案)
    void forceRefresh();

    // 直接操作 API (给外部动画使用)
    void displayEyes(const byte eyeL[], const byte eyeR[]);
    
    void clear();

    // 图案定义
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

private:
    LedControl* m_lc = nullptr;
    int m_lastLooking = -1;
};

#endif
