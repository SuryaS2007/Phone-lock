#include <Arduino.h>
#include <LiquidCrystal.h>

// ==========================================
// LCD
// ==========================================

LiquidCrystal lcd(23, 22, 21, 19, 18, 5);

// ==========================================
// CENTER EYE
// ==========================================
uint8_t centerTop[8] = {
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10101,
    0b10101,
    0b10101
};

uint8_t centerBottom[8] = {
    0b10101,
    0b10101,
    0b10101,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

// ==========================================
// LOOK LEFT
// ==========================================
uint8_t leftTop[8] = {
    0b11110,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b11001,
    0b11001,
    0b11001
};

uint8_t leftBottom[8] = {
    0b11001,
    0b11001,
    0b11001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

// ==========================================
// LOOK RIGHT
// ==========================================
uint8_t rightTop[8] = {
    0b01111,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10011,
    0b10011,
    0b10011
};

uint8_t rightBottom[8] = {
    0b10011,
    0b10011,
    0b10011,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

// ==========================================
// BLINK
// ==========================================
uint8_t blinkTop[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b11111,
    0b11111,
    0b00000
};

uint8_t blinkBottom[8] = {
    0b00000,
    0b11111,
    0b11111,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

// ==========================================
// HAPPY (Squinting up ^^)
// ==========================================
uint8_t happyTop[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b00000
};

uint8_t happyBottom[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

// ==========================================
// SURPRISED / SHOCKED (Tiny pupils)
// ==========================================
uint8_t surprisedTop[8] = {
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10101,
    0b10101
};

uint8_t surprisedBottom[8] = {
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
// SLEEPING (Closed resting eyelids)
// ==========================================
uint8_t sleepTop[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b10001,
    0b01110,
    0b00000
};

uint8_t sleepBottom[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

// ==========================================
// DRAW EYES
// ==========================================
void showEyes(uint8_t topCharacter, uint8_t bottomCharacter)
{
    // LEFT EYE
    lcd.setCursor(4, 0);
    lcd.write(topCharacter);

    lcd.setCursor(4, 1);
    lcd.write(bottomCharacter);

    // RIGHT EYE
    lcd.setCursor(11, 0);
    lcd.write(topCharacter);

    lcd.setCursor(11, 1);
    lcd.write(bottomCharacter);
}

// ==========================================
// DYNAMIC EMOTION LOADER
// ==========================================
void setEmotion(uint8_t* topArray, uint8_t* bottomArray) 
{
    // Overwrite slots 0 and 1 in the LCD's memory
    lcd.createChar(0, topArray);
    lcd.createChar(1, bottomArray);
    
    // Display the newly loaded characters
    showEyes(0, 1);
}

// ==========================================
// BLINK ANIMATION
// ==========================================
void blink()
{
    // Close
    showEyes(6, 7);
    delay(120);

    // Open (Assumes slots 0 and 1 currently hold the center eyes)
    showEyes(0, 1);
    delay(150);
}

// ==========================================
// SETUP
// ==========================================
void setup()
{
    lcd.begin(16, 2);

    // Load initial characters into LCD memory slots 0-7
    lcd.createChar(0, centerTop);
    lcd.createChar(1, centerBottom);
    lcd.createChar(2, leftTop);
    lcd.createChar(3, leftBottom);
    lcd.createChar(4, rightTop);
    lcd.createChar(5, rightBottom);
    lcd.createChar(6, blinkTop);
    lcd.createChar(7, blinkBottom);

    lcd.clear();

    // Start centered
    showEyes(0, 1);

    delay(1000);
}

// ==========================================
// ANIMATION
// ==========================================
void loop()
{
    // --------------------------------------
    // CENTER
    // --------------------------------------
    // Make sure center eyes are loaded into memory just in case 
    // a previous emotion overwrote them.
    setEmotion(centerTop, centerBottom);
    delay(1000);

    // --------------------------------------
    // LOOK LEFT
    // --------------------------------------
    showEyes(2, 3);
    delay(900);

    // --------------------------------------
    // CENTER
    // --------------------------------------
    showEyes(0, 1);
    delay(700);

    // --------------------------------------
    // LOOK RIGHT
    // --------------------------------------
    showEyes(4, 5);
    delay(900);

    // --------------------------------------
    // CENTER
    // --------------------------------------
    showEyes(0, 1);
    delay(1000);

    // --------------------------------------
    // BLINK
    // --------------------------------------
    blink();
    delay(1000);

    // --------------------------------------
    // HAPPY! ^^
    // --------------------------------------
    // This temporarily overwrites the center eye memory slots (0 and 1)
    setEmotion(happyTop, happyBottom);
    delay(2000);

    // --------------------------------------
    // IDLE
    // --------------------------------------
    // Reset back to center eyes so the next loop starts correctly
    setEmotion(centerTop, centerBottom);
    delay(1500);

    blink();
    delay(200);
    blink();
    delay(500);
    
    // Fall asleep
    setEmotion(sleepTop, sleepBottom);
    delay(3000);

    // --------------------------------------
    // GO TO SLEEP
    // --------------------------------------
    setEmotion(sleepTop, sleepBottom);
    delay(3000);
}