#include <vector>
#include <string>

#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>

#include "common.h"
#include "CommandTypes.h"
#include "Protocol.h"

#include "ble/BLEManager.h"
#include "data_transfer/DataTransferManager.h"

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

/* Include the resulting header */
#include BOOT_INCLUDE_HEADER
/* This results in: #define BOOT_IMAGE_BITS {BOOT_IMAGE}_bits */
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
    /*reset =*/U8X8_PIN_NONE,
    /*clock =*/OLED_SCL,
    /*data  =*/OLED_SDA
  );
#endif

BLEManager ble;

// State
bool isBlinking = false;
bool invert = false;
unsigned long previousBlinkMillis = 0;
const long blinkInterval = 500;

std::vector<std::string> badges;
uint8_t currentBadge = 0;
bool needsRedraw = true;
unsigned long previousBadgeMillis = 0;
const long badgeInterval = 5000; // 5 sec

// --- COMMAND 1: Toggle LED (OpCode 0x01) ---
void cmd_led(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    if (len >= 1) {
        isBlinking = (data[0] != 0);
        invert = !isBlinking ? false : invert;
        
        // Send acknowledgment back
        uint8_t response[] = {OpCodes::SET_LED, data[0]}; 
        pChar->setValue(response, 2);
        pChar->notify();
    }
}

// --- COMMAND 3: List LittleFS Directory (OpCode 0x02) ---
void cmd_get_status(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    // 1. Prepare the packet: [OpCode, State]
    // We reuse GET_STATUS so the Flutter app knows which request this is answering
    uint8_t response[] = { OpCodes::GET_STATUS, isBlinking }; 
    
    // 2. Push to BLE stack and alert the phone
    pChar->setValue(response, 2);
    pChar->notify();
}

// --- COMMAND 3: List LittleFS Directory (OpCode 0x04) ---
void cmd_list_files(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    File root = LittleFS.open("/");
    if (!root) {
        uint8_t err[] = {OpCodes::LIST_FILES, OpCodes::STATUS_ERR};
        pChar->setValue(err, 2);
        pChar->notify();
        return;
    }

    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        uint8_t response[32];
        
        response[0] = OpCodes::FILE_ENTRY; // Using the Enum-style struct
        memcpy(&response[1], fileName.c_str(), fileName.length());

        pChar->setValue(response, fileName.length() + 1);
        pChar->notify();
        file = root.openNextFile();
        delay(15);
    }
    
    uint8_t end[] = {OpCodes::LIST_FILES, OpCodes::STATUS_OK};
    pChar->setValue(end, 2);
    pChar->notify();
}

// --- THE REGISTRY ---
static const CommandEntry bleCommands[] = {
    {OpCodes::SET_LED,    cmd_led},
    {OpCodes::GET_STATUS, cmd_get_status},
    {OpCodes::LIST_FILES, cmd_list_files},
    /* */
    { OpCodes::FILE_UPLOAD_START, DataTransferManager::handleStartFile },
    { OpCodes::OTA_START,         DataTransferManager::handleStartOTA },
    { OpCodes::UPLOAD_DATA,       DataTransferManager::handleData },
    { OpCodes::UPLOAD_END,        DataTransferManager::handleEnd }
};

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

void loadImageNames() {
    badges.clear();

    // Open the root directory
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println(" - failed to open directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        // Add the filename to our vector
        badges.push_back(std::string("/") + file.name());
        
        // Move to the next file
        file = root.openNextFile();
    }
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
  loadImageNames();

  ble.setRegistry(bleCommands, sizeof(bleCommands) / sizeof(CommandEntry));

  ble.begin(DEVICE_NAME, SERVICE_UUID, COMMAND_CHARACTERISTIC_UUID);

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
        u8g2.clearBuffer();
        // U8g2's drawXBM is designed for this specific byte format
        u8g2.drawXBM(x, y, w, h, buffer);
        u8g2.sendBuffer();
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
  ble.loop();

  if (currentMillis - previousBadgeMillis >= badgeInterval)
  {
    previousBadgeMillis = currentMillis;
    currentBadge = (currentBadge + 1) % badges.size();
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
    drawImage(badges[currentBadge].c_str(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    needsRedraw = false;
  }
}