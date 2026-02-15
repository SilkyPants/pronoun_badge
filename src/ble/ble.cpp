
#include "ble.h"

#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

//TODO: Remove me later in favor of better state management
#include "common.h"


BLECharacteristic *pStatusCharacteristic = nullptr;
bool deviceConnected = false;

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