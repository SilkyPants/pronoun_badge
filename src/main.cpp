#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <LittleFS.h>

#include "common.h"
#include "ble/ble.h"

#define OLED_GND 20
#define OLED_VCC 10
#define OLED_SCL 9
#define OLED_SDA 8

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R2, 
  /* reset=*/ U8X8_PIN_NONE, 
  /*clock =*/ OLED_SCL, 
  /*data =*/ OLED_SDA
);

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

// State
bool isBlinking = false;
bool invert = false;
unsigned long previousBlinkMillis = 0;
const long blinkInterval = 500;

uint8_t currentBadge = 0;
unsigned long previousBadgeMillis = 0;
const long badgeInterval = 5000; // 5 sec

void printLittleFSStats() {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();

    Serial.println("--- LittleFS Stats ---");
    Serial.print("Total Space: ");
    Serial.print(total);
    Serial.println(" bytes");

    Serial.print("Used Space:  ");
    Serial.print(used);
    Serial.println(" bytes");
    
    // Calculate percentage
    float usage = ((float)used / (float)total) * 100;
    Serial.printf("Usage:       %.2f%%\n", usage);
    Serial.println("----------------------");
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void setup() {

  Serial.begin(115200);

  // Create power for the OLED
  pinMode(OLED_GND, OUTPUT);
  digitalWrite(OLED_GND, LOW); // GND
  pinMode(OLED_VCC, OUTPUT);
  digitalWrite(OLED_VCC, HIGH); // VCC

  delay(100); // Wait for OLED to stabilize

  u8g2.begin();

  initBLE();
  
  // Initialize LittleFS
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    printLittleFSStats();
    readFile(LittleFS, "/test.txt");
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