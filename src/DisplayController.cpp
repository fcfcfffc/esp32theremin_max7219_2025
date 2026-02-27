#include "DisplayController.h"

// ========================================================
// ======= 眼睛图案 (const static, 存储在flash) ===========
// ========================================================

const byte DisplayController::EYE_OPEN[]         = { 0b00111100,0b01000010,0b10000001,0b10011001,0b10011001,0b10000001,0b01000010,0b00111100 };
const byte DisplayController::EYE_CLOSED[]       = { 0b00011000,0b00011000,0b00011000,0b00011000,0b00011000,0b00011000,0b00011000,0b00000000 };
const byte DisplayController::EYE_PARTIAL[]      = { 0b00000000,0b00111100,0b00100100,0b00110100,0b00110100,0b00100100,0b00111100,0b00000000 };
const byte DisplayController::EYE_PARTIAL_OPEN[] = { 0b00000000,0b00111100,0b01000010,0b01011010,0b01011010,0b01000010,0b00111100,0b00000000 };
const byte DisplayController::EYE_SLIGHT_LEFT[]  = { 0b00111100,0b01000010,0b10011001,0b10011001,0b10000001,0b10000001,0b01000010,0b00111100 };
const byte DisplayController::EYE_LEFT[]         = { 0b00111100,0b01011010,0b10011001,0b10000001,0b10000001,0b10000001,0b01000010,0b00111100 };
const byte DisplayController::EYE_REAL_LEFT[]   = { 0b00111100,0b01011010,0b10000001,0b10000001,0b10000001,0b01000010,0b00111100,0b00000000 };
const byte DisplayController::EYE_SLIGHT_RIGHT[] = { 0b00111100,0b01000010,0b10000001,0b10000001,0b10011001,0b10011001,0b01000010,0b00111100 };
const byte DisplayController::EYE_RIGHT[]        = { 0b00111100,0b01000010,0b10000001,0b10000001,0b10000001,0b10011001,0b01011010,0b00111100 };
const byte DisplayController::EYE_REAL_RIGHT[]   = { 0b00000000,0b01111110,0b10000001,0b10000001,0b10000001,0b10000001,0b01011010,0b00111100 };

// ========================================================
// ======= 构造函数与初始化 ==============================
// ========================================================

DisplayController::DisplayController() {
    // m_lc 在 begin() 中初始化
}

bool DisplayController::begin() {
    // 初始化LedControl (动态分配)
    m_lc = new LedControl(config.ledDinPin, config.ledClkPin, config.ledCsPin, config.ledModuleCount);
    if (!m_lc) {
        Serial.println("ERROR: LedControl allocation failed");
        return false;
    }
    
    for(int i = 0; i < config.ledModuleCount; i++) {
        m_lc->shutdown(i, false);
        m_lc->setIntensity(i, 8);
        m_lc->clearDisplay(i);
    }
    
    Serial.println("Display Controller Started");
    return true;
}

// ========================================================
// ======= 显示控制 =======================================
// ========================================================

void DisplayController::displayEyes(const byte eyeL[], const byte eyeR[]) {
    if (!m_lc) return;
    for(int i = 0; i < 4; i++) {
        for(int row = 0; row < 8; row++) {
            m_lc->setRow(i, row, eyeL[row]);
        }
    }
    for(int i = 4; i < 8; i++) {
        for(int row = 0; row < 8; row++) {
            m_lc->setRow(i, row, eyeR[row]);
        }
    }
}

void DisplayController::updateEyes(int looking) {
    // 眨眼动画进行中时不更新正常眼睛
    if (m_blinkState != BLINK_IDLE) return;
    
    // 脏标记: 仅在looking变化时刷新LED
    if (looking == m_lastLooking) return;
    m_lastLooking = looking;
    
    switch(looking) {
        case 8: displayEyes(EYE_REAL_LEFT, EYE_REAL_LEFT); break;
        case 7: displayEyes(EYE_LEFT, EYE_LEFT); break;
        case 6: displayEyes(EYE_SLIGHT_LEFT, EYE_SLIGHT_LEFT); break;
        case 5: displayEyes(EYE_OPEN, EYE_OPEN); break;
        case 4: displayEyes(EYE_SLIGHT_RIGHT, EYE_SLIGHT_RIGHT); break;
        case 3: displayEyes(EYE_RIGHT, EYE_RIGHT); break;
        case 2: displayEyes(EYE_REAL_RIGHT, EYE_REAL_RIGHT); break;
        case 1: displayEyes(EYE_PARTIAL_OPEN, EYE_PARTIAL_OPEN); break;
        case 0:
        default:
            displayEyes(EYE_PARTIAL, EYE_PARTIAL);
            // 随机触发非阻塞眨眼 (包括 looking=0)
            if (random(1, 20) == 1) {
                m_blinkState = BLINK_PARTIAL;
                m_blinkStartTime = millis();
                displayEyes(EYE_CLOSED, EYE_CLOSED);
            }
            break;
    }
}

void DisplayController::processBlink() {
    if (m_blinkState == BLINK_IDLE) return;
    
    unsigned long elapsed = millis() - m_blinkStartTime;
    
    switch(m_blinkState) {
        case BLINK_PARTIAL:
            if (elapsed >= 100) {
                displayEyes(EYE_CLOSED, EYE_CLOSED);
                m_blinkState = BLINK_CLOSED;
                m_blinkStartTime = millis();
            }
            break;
        case BLINK_CLOSED:
            if (elapsed >= 50) {
                displayEyes(EYE_PARTIAL, EYE_PARTIAL);
                m_blinkState = BLINK_REOPEN;
                m_blinkStartTime = millis();
            }
            break;
        case BLINK_REOPEN:
            if (elapsed >= 50) {
                m_blinkState = BLINK_IDLE;
                m_lastLooking = -1;  // 强制下次updateEyes刷新
            }
            break;
        default:
            break;
    }
}

void DisplayController::clear() {
    if (!m_lc) return;
    for(int i = 0; i < config.ledModuleCount; i++) {
        m_lc->clearDisplay(i);
    }
}
