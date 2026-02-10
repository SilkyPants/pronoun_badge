#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#define OLED_GND 20
#define OLED_VCC 10
#define OLED_SCL 9
#define OLED_SDA 8

// OPTION A: Standard SSD1306 (Try this first)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE, /*clock =*/OLED_SCL, /*data =*/OLED_SDA);

/// BLE

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Unique IDs
#define DEVICE_NAME "Pronoun Badge"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WRITE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define READ_CHARACTERISTIC_UUID "a1b2c3d4-e5f6-47a8-b9c0-d1e2f3a4b5c6"

// State
bool isBlinking = false;
unsigned long previousBlinkMillis = 0;
const long blinkInterval = 500;

BLECharacteristic *pStatusCharacteristic = nullptr;
bool deviceConnected = false;

void initBLE();
///

#include "danni.h"
#include "she_her_data.h"
#include "they_them_data.h"

#define NUM_BADGES 2
#define BADGE_WIDTH 128
#define BADGE_HEIGHT 64

const uint8_t *badges[] = {
    danni_bits,
    she_her_bits};

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

bool invert = false;

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


// Server Callback to handle connection events
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("Device Connected");
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("Device Disconnected");
    // Restart advertising so we can reconnect
    BLEDevice::startAdvertising();
  }
};

// Characteristic Callback to handle data sent from phone
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    // Get the raw data pointer and length
    uint8_t *data = pCharacteristic->getData();
    size_t len = pCharacteristic->getLength();

    if (len > 0)
    {
      uint8_t command = data[0]; // Grab the first byte

      // 0x01 (Hex/Byte) instead of '1' (Text)
      if (command == 0x01)
      {
        isBlinking = true;
        pStatusCharacteristic->setValue("Blinking");
        pStatusCharacteristic->notify();
      }
      else if (command == 0x00)
      {
        isBlinking = false;
        invert = false;
        pStatusCharacteristic->setValue("Stopped");
        pStatusCharacteristic->notify();
      }
    }
  }
};

MyServerCallbacks myServerCallbacks;
MyCharacteristicCallbacks myCharCallbacks;

BLEDescriptor controlLabelDescriptor((uint16_t)0x2901);
BLEDescriptor statusLabelDescriptor((uint16_t)0x2901);
BLE2902 notifyDescriptor;

void initBLE()
{

  // Initialize BLE
  BLEDevice::init(DEVICE_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(&myServerCallbacks);

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Control Characteristic
  BLECharacteristic *pControlChar = pService->createCharacteristic(
      WRITE_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  pControlChar->setCallbacks(&myCharCallbacks);

  controlLabelDescriptor.setValue("LED Blink Control (1=On, 0=Off)");
  pControlChar->addDescriptor(&controlLabelDescriptor);

  // Status Characteristic
  pStatusCharacteristic = pService->createCharacteristic(
      READ_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_NOTIFY);

  statusLabelDescriptor.setValue("Current Blinking Status");
  pStatusCharacteristic->addDescriptor(&statusLabelDescriptor);

// Configure the global notify descriptor (2902)
  pStatusCharacteristic->addDescriptor(&notifyDescriptor);
  pStatusCharacteristic->setValue("Stopped");

  pService->start();

  // Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("System online and advertising...");
}