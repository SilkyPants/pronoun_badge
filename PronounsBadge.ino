#include <Arduino.h>
#include <U8g2lib.h>

// #ifdef U8X8_HAVE_HW_SPI
// #include <SPI.h>
// #endif

#include "danni.h"
#include "she_her_data.h"
#include "they_them_data.h"

// U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* clock= 13, data= 11,*/ /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);


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

void setup(void) {
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


