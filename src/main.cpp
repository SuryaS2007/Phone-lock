#include <Arduino.h>
#include <LiquidCrystal.h>

// ==========================================
// LCD
// ==========================================

LiquidCrystal lcd(23, 22, 21, 19, 18, 5);


// ==========================================
// NORMAL EYES
// ==========================================

// Center - top
byte normalTop[8] = {
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10101,
    0b10101
};

// Center - bottom
byte normalBottom[8] = {
    0b10101,
    0b10101,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};


// ==========================================
// LOOK LEFT
// ==========================================

// Whole eye shifted toward LEFT
byte leftTop[8] = {
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b11001,
    0b11001
};

byte leftBottom[8] = {
    0b11001,
    0b11001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};


// ==========================================
// LOOK RIGHT
// ==========================================

byte rightTop[8] = {
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10011,
    0b10011
};

byte rightBottom[8] = {
    0b10011,
    0b10011,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};


// ==========================================
// SLEEPY EYES
// ==========================================

// Drooping upper eyelid
byte sleepyTop[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b11111,
    0b11111,
    0b11011,
    0b11011
};

// Small opening underneath
byte sleepyBottom[8] = {
    0b11011,
    0b11011,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110,
    0b00000
};


// ==========================================
// VERY SLEEPY / ALMOST CLOSED
// ==========================================

byte verySleepyTop[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b11111,
    0b11111,
    0b00000,
    0b00000,
    0b00000
};

byte verySleepyBottom[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b11111,
    0b11111,
    0b00000,
    0b00000,
    0b00000
};


// ==========================================
// SHOW EYES
// ==========================================

void showEyes(byte top, byte bottom)
{
    // Left eye
    lcd.setCursor(4, 0);
    lcd.write(top);

    lcd.setCursor(4, 1);
    lcd.write(bottom);

    // Right eye
    lcd.setCursor(11, 0);
    lcd.write(top);

    lcd.setCursor(11, 1);
    lcd.write(bottom);
}


// ==========================================
// SETUP
// ==========================================

void setup()
{
    lcd.begin(16, 2);

    lcd.createChar(0, normalTop);
    lcd.createChar(1, normalBottom);

    lcd.createChar(2, leftTop);
    lcd.createChar(3, leftBottom);

    lcd.createChar(4, rightTop);
    lcd.createChar(5, rightBottom);

    lcd.createChar(6, sleepyTop);
    lcd.createChar(7, sleepyBottom);

    lcd.clear();

    showEyes(0, 1);

    delay(1000);
}


// ==========================================
// ANIMATION
// ==========================================

void loop()
{
    // Normal
    showEyes(0, 1);
    delay(1200);


    // Slowly look LEFT
    showEyes(2, 3);
    delay(1000);


    // Center
    showEyes(0, 1);
    delay(700);


    // Slowly look RIGHT
    showEyes(4, 5);
    delay(1000);


    // Center
    showEyes(0, 1);
    delay(700);


    // Get sleepy
    showEyes(6, 7);
    delay(1500);


    // Wake up
    showEyes(0, 1);
    delay(2000);
}