#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "common.h"
#include "ble/ble.h"

#define OLED_GND 20
#define OLED_VCC 10
#define OLED_SCL 9
#define OLED_SDA 8

// OPTION A: Standard SSD1306 (Try this first)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE, /*clock =*/OLED_SCL, /*data =*/OLED_SDA);

#include "danni.h"
#include "she_her_data.h"
#include "they_them_data.h"
#include "tantalus_south.h"

#define NUM_BADGES 3
#define BADGE_WIDTH 128
#define BADGE_HEIGHT 64

const uint8_t *badges[] = {
    danni_bits,
    she_her_bits,
    tantalus_south_bits
};

uint8_t currentBadge = 0;
unsigned long previousBadgeMillis = 0;
const long badgeInterval = 5000; // 5 sec

void setup()
{

  Serial.begin(115200);

  // Create power for the OLED
  pinMode(OLED_GND, OUTPUT);
  digitalWrite(OLED_GND, LOW); // GND
  pinMode(OLED_VCC, OUTPUT);
  digitalWrite(OLED_VCC, HIGH); // VCC

  delay(100); // Wait for OLED to stabilize

  u8g2.begin();

  initBLE();
}

void loop(void)
{
  u8g2.clearBuffer();
  u8g2.setColorIndex(invert && isBlinking ? 0 : 1);
  u8g2.drawXBMP(0, 0, BADGE_WIDTH, BADGE_HEIGHT, badges[currentBadge]);
  u8g2.sendBuffer();

  unsigned long currentMillis = millis();
  if (isBlinking && currentMillis - previousBlinkMillis >= blinkInterval)
  {
    previousBlinkMillis = currentMillis;
    invert = !invert;
  }

  if (currentMillis - previousBadgeMillis >= badgeInterval)
  {
    previousBadgeMillis = currentMillis;
    currentBadge = (currentBadge + 1) % NUM_BADGES;
  }
}