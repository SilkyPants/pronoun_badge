#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>

#include "common.h"
#include "ble/BLEManager.h"

#ifdef BOOT_IMAGE
/* This all assumes that:
- Include file lives under boot_images
- is named {BOOT_IMAGE}.h
- has the data defined as {BOOT_IMAGE}_bits

*/

/* Include the dynamic BOOT_IMAGE header file */
/* We build the path as a single token sequence, then stringize it.
   Note: No quotes inside the 'PATH' macro. */
#define BOOT_IMAGES_PATH(img) boot_images/img.h
#define BOOT_INCLUDE_HEADER STR(BOOT_IMAGES_PATH(BOOT_IMAGE))

/* 4. Include the resulting header */
#include BOOT_INCLUDE_HEADER
/* This results in: #define BOOT_IMAGE_BITS image_name_bits */
#define BOOT_IMAGE_BITS GLUE(BOOT_IMAGE, _bits)

#endif


#ifdef USE_TFT_ESPI
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
#endif

#ifdef USE_SSD1315
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R2,
    /* reset=*/U8X8_PIN_NONE,
    /*clock =*/OLED_SCL,
    /*data =*/OLED_SDA);
#endif

#ifdef SCREEN_COLOUR
const char* badges[] = {
    "/color-test.bin"
};
#else
const char* badges[] = {
    "/danni.bin",
    "/she_her.bin",
    "/tantalus_south.bin"
};
#endif

BLEManager ble;

// State
bool isBlinking = false;
bool invert = false;
unsigned long previousBlinkMillis = 0;
const long blinkInterval = 500;

uint8_t currentBadge = 0;
bool needsRedraw = true;
unsigned long previousBadgeMillis = 0;
const long badgeInterval = 5000; // 5 sec

void printLittleFSStats()
{
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

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available())
  {
    Serial.write(file.read());
  }
  Serial.write("\n\0");
  file.close();
}

void setup()
{

  Serial.begin(115200);
  while (!Serial); // Wait for Serial port to connect
  delay(1000);
  Serial.println("System Initialized...");

#ifdef USE_SSD1315
  // Create power for the OLED
  pinMode(OLED_GND, OUTPUT);
  digitalWrite(OLED_GND, LOW); // GND
  pinMode(OLED_VCC, OUTPUT);
  digitalWrite(OLED_VCC, HIGH); // VCC

  delay(100); // Wait for OLED to stabilize

  u8g2.begin();
#endif

#ifdef USE_TFT_ESPI
  // Initialize the display
  tft.init();
  tft.setRotation(1); // Landscape orientation

  tft.fillScreen(TFT_WHITE);
#endif

#ifdef BOOT_IMAGE
  #ifdef USE_TFT_ESPI
    tft.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BOOT_IMAGE_BITS);
  #else
    u8g2.clearBuffer();
    u8g2.drawXBMP(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BOOT_IMAGE_BITS);

    u8g2.sendBuffer();
  #endif
#endif


  // Initialize LittleFS
  if (!LittleFS.begin())
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  printLittleFSStats();
  readFile(LittleFS, "/test.txt");

  ble.begin(DEVICE_NAME, SERVICE_UUID);
  
  ble.addLambdaCharacteristic<bool>(
      BLINKING_CHARACTERISTIC_UUID,
      [](bool val)
      {
        isBlinking = val;
        invert = !isBlinking ? false : invert;
      },
      []()
      {
        return isBlinking;
      });

  ble.addHybridLambda(
      BLINKING_HYBRID_CHARACTERISTIC_UUID,
      [](bool val)
      {
        isBlinking = val;
        invert = !isBlinking ? false : invert;
      },
      []()
      {
        return isBlinking ? "Blinking"
                          : "Stopped";
      });

  ble.start();

#ifdef BOOT_IMAGE
  delay(3000);
#endif
  previousBadgeMillis = previousBlinkMillis = millis();
}

void drawImage(const char* filename, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    File file = LittleFS.open(filename, "r");
    if (!file) return;

#ifdef USE_SSD1315

    size_t size = file.size();
    uint8_t* buffer = (uint8_t*)malloc(size);

    if (buffer) {
        file.read(buffer, size);
        // U8g2's drawXBM is designed for this specific byte format
        u8g2.drawXBM(x, y, w, h, buffer);
        free(buffer);
    }
#endif

#ifdef USE_TFT_ESPI
    tft.startWrite();
    tft.setAddrWindow(x, y, w, h);

    uint16_t lineBuffer[w]; 
    for (int row = 0; row < h; row++) {
        file.read((uint8_t*)lineBuffer, w * 2); // 2 bytes per pixel in RGB565
        tft.pushImage(x, y + row, w, 1, lineBuffer);
    }

    tft.endWrite();

#endif

    file.close();
}

void loop(void)
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousBadgeMillis >= badgeInterval)
  {
    previousBadgeMillis = currentMillis;
    currentBadge = (currentBadge + 1) % std::size(badges);
    needsRedraw = true;
  }

  if (isBlinking && currentMillis - previousBlinkMillis >= blinkInterval)
  {
    previousBlinkMillis = currentMillis;

#ifdef USE_SSD1315
    // Draw a solid box over the SAME area as the image using XOR mode
    // Because it's XOR:
    // First time this runs: Image is inverted.
    // Second time this runs: Image is restored to original!
    u8g2.setDrawColor(2); 
    u8g2.drawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT); 
    u8g2.sendBuffer();
    
#endif

#ifdef USE_TFT_ESPI
    tft.invertDisplay(invert);
#endif

    invert = !invert;
  }

  if (needsRedraw)
  {
#ifdef USE_SSD1315
    u8g2.clearBuffer();
    
    drawImage(badges[currentBadge], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    u8g2.sendBuffer();
#endif

#ifdef USE_TFT_ESPI
    drawImage(badges[currentBadge], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
#endif

    needsRedraw = false;
  }
}