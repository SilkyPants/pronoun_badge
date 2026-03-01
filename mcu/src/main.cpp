#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>

#include "common.h"
#include "display/Display.h"
#include "ble/BLEManager.h"
#include "data_transfer/DataTransferManager.h"
#include "data_transfer/CommandTypes.h"
#include "data_transfer/Protocol.h"

// State
bool isBlinking = false;
bool invert = false;
unsigned long previousBlinkMillis = 0;
const long blinkInterval = 500;

#define MAX_FILES 20
#define MAX_FILENAME_LEN 32

char badgeList[MAX_FILES][MAX_FILENAME_LEN];
int badgeCount = 0;
uint8_t currentBadge = 0;

bool needsRedraw = true;
unsigned long previousBadgeMillis = 0;
const long badgeInterval = 5000; // 5 sec

// --- COMMAND 1: Toggle Flashing (OpCode 0x01) ---
void cmd_flash(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    if (len >= 1) {
        isBlinking = (data[0] != 0);
        invert = !isBlinking ? false : invert;
        
        // Send acknowledgment back
        uint8_t response[] = {OpCodes::SET_FLASH, data[0]}; 
        pChar->setValue(response, 2);
        pChar->notify();
    }
}

struct __attribute__((packed)) StatusPacket {
    const uint8_t opCode = OpCodes::GET_STATUS; // 1 byte
    uint8_t flashingState;                      // 1 byte
    uint32_t totalFS;                           // 4 bytes
    uint32_t usedFS;                            // 4 bytes
};

// --- COMMAND 3: List LittleFS Directory (OpCode 0x02) ---
void cmd_get_status(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    // 1. Prepare the packet:
    StatusPacket packet;
    packet.flashingState = isBlinking;
    packet.totalFS = isBlinking;
    packet.usedFS = isBlinking;

    // 2. Push to BLE stack and alert the phone
    pChar->setValue((uint8_t*)&packet, sizeof(StatusPacket));
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
    {OpCodes::SET_FLASH,    cmd_flash},
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
    badgeCount = 0;
    File root = LittleFS.open("/");
    
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file && badgeCount < MAX_FILES) {
        // Copy filename directly into our pre-allocated slots
        strncpy(badgeList[badgeCount], file.name(), MAX_FILENAME_LEN - 1);
        
        // Ensure null-termination
        badgeList[badgeCount][MAX_FILENAME_LEN - 1] = '\0';
        
        badgeCount++;
        file = root.openNextFile();
    }
    root.close();
}

void setup()
{

  Serial.begin(115200);
  while (!Serial); // Wait for Serial port to connect
  delay(1000);
  Serial.println("System Initialized...");

  initDisplay();

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

void loop(void)
{
  unsigned long currentMillis = millis();
  ble.loop();

  if (currentMillis - previousBadgeMillis >= badgeInterval)
  {
    previousBadgeMillis = currentMillis;
    currentBadge = (currentBadge + 1) % badgeCount;
    needsRedraw = true;
  }

  if (isBlinking && currentMillis - previousBlinkMillis >= blinkInterval)
  {
    previousBlinkMillis = currentMillis;

    invertDisplay(invert);

    invert = !invert;
  }

  if (needsRedraw)
  {
    drawImage(badgeList[currentBadge], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    needsRedraw = false;
  }
}