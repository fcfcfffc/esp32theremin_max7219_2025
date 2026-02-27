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
const byte DisplayController::EYE_REAL_LEFT[]    = { 0b00111100,0b01011010,0b10000001,0b10000001,0b10000001,0b01000010,0b00111100,0b00000000 };
const byte DisplayController::EYE_SLIGHT_RIGHT[] = { 0b00111100,0b01000010,0b10000001,0b10000001,0b10011001,0b10011001,0b01000010,0b00111100 };
const byte DisplayController::EYE_RIGHT[]        = { 0b00111100,0b01000010,0b10000001,0b10000001,0b10000001,0b10011001,0b01011010,0b00111100 };
const byte DisplayController::EYE_REAL_RIGHT[]   = { 0b00000000,0b01111110,0b10000001,0b10000001,0b10000001,0b10000001,0b01011010,0b00111100 };

DisplayController::DisplayController() {}

bool DisplayController::begin() {
    m_lc = new LedControl(config.ledDinPin, config.ledClkPin, config.ledCsPin, config.ledModuleCount);
    if (!m_lc) return false;
    for(int i = 0; i < config.ledModuleCount; i++) {
        m_lc->shutdown(i, false);
        m_lc->setIntensity(i, 8);
        m_lc->clearDisplay(i);
    }
    return true;
}

void DisplayController::displayEyes(const byte eyeL[], const byte eyeR[]) {
    if (!m_lc) return;
    for(int i = 0; i < 4; i++) {
        for(int row = 0; row < 8; row++) m_lc->setRow(i, row, eyeL[row]);
    }
    for(int i = 4; i < 8; i++) {
        for(int row = 0; row < 8; row++) m_lc->setRow(i, row, eyeR[row]);
    }
}

void DisplayController::updateEyes(int looking) {
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
        default: displayEyes(EYE_PARTIAL, EYE_PARTIAL); break;
    }
}

void DisplayController::forceRefresh() {
    m_lastLooking = -1;
}

void DisplayController::clear() {
    if (!m_lc) return;
    for(int i = 0; i < config.ledModuleCount; i++) m_lc->clearDisplay(i);
}
