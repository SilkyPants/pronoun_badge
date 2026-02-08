#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#define OLED_GND 20
#define OLED_VCC 10
#define OLED_SCL 9
#define OLED_SDA 8

// OPTION A: Standard SSD1306 (Try this first)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE, /*clock =*/ OLED_SCL, /*data =*/ OLED_SDA);

// OPTION B: If Option A is still shifted/wrapping, uncomment this and comment out Option A
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

#include "danni.h"
#include "she_her_data.h"
#include "they_them_data.h"

#define NUM_BADGES    2
#define BADGE_WIDTH 128
#define BADGE_HEIGHT 64

const uint8_t* badges[] = {
  danni_bits,
  she_her_bits
};

uint8_t currentBadge = 0;
uint8_t loopCount = 0;
#define NUM_LOOPS 5

void setup() {

  // Create power for the OLED
  pinMode(OLED_GND, OUTPUT); digitalWrite(OLED_GND, LOW);  // GND
  pinMode(OLED_VCC, OUTPUT); digitalWrite(OLED_VCC, HIGH); // VCC
  
  delay(100); // Wait for OLED to stabilize
  
  u8g2.begin();
}

bool invert = false;

void loop(void) {
  u8g2.clearBuffer();
  u8g2.setColorIndex(invert ? 1 : 0);
  u8g2.drawXBMP(0, 0, BADGE_WIDTH, BADGE_HEIGHT, badges[currentBadge]);
  u8g2.sendBuffer();
  delay(1000);
  invert = !invert;
  loopCount++;

  if (loopCount >= NUM_LOOPS)
  {
    loopCount = 0;
    currentBadge = (currentBadge + 1) % NUM_BADGES;
  }
}