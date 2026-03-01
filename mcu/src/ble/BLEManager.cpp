#include "BLEManager.h"
#include <Arduino.h>

#include "data_transfer/DataTransferManager.h"

BLEManager ble;

void BLEManager::begin(const char* deviceName, const char* serviceUUID, const char* commandCharUUID, const int mtu) {
    BLEDevice::init(deviceName);
    BLEDevice::setMTU(mtu);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);
    pService = pServer->createService(serviceUUID);

    // Setup command characteristic
    uint32_t props = BLECharacteristic::PROPERTY_WRITE | 
                     /* BLECharacteristic::PROPERTY_READ | */ 
                     BLECharacteristic::PROPERTY_NOTIFY;

    pCommandCharacteristic = pService->createCharacteristic(commandCharUUID, props);
    pCommandCharacteristic->addDescriptor(new BLE2902());
    pCommandCharacteristic->setCallbacks(this);
}

void BLEManager::start() {
    if (pService) {
         pService->start();

        // Advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(pService->getUUID());
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMaxPreferred(0x12);
        BLEDevice::startAdvertising();

        Serial.println("System online and advertising...");
    }
}

void BLEManager::loop() 
{
    DataTransferManager::loop(pCommandCharacteristic);
}

void BLEManager::onConnect(BLEServer *pServer)
{
    deviceConnected = true;
    Serial.println("Device Connected");
};

void BLEManager::onDisconnect(BLEServer *pServer)
{
    deviceConnected = false;
    Serial.println("Device Disconnected");
    // Restart advertising so we can reconnect
    BLEDevice::startAdvertising();
}

void BLEManager::onMtuChanged(BLEServer* pServer, uint16_t mtu) 
{
    Serial.printf("MTU exchanged: %d\n", mtu);
}

void BLEManager::cleanup() {
    // 1. Stop advertising and service
    pServer->getAdvertising()->stop();
    pService->stop();
    
    Serial.println("BLE Resources Freed Safely");
}

void BLEManager::onWrite(BLECharacteristic* pCharacteristic) {
    uint8_t* pData = pCharacteristic->getData();
    size_t len = pCharacteristic->getLength();
    
    if (len < 1 || _registry == nullptr) return;

    uint8_t opCode = pData[0];
    const uint8_t* payload = (len > 1) ? &pData[1] : nullptr;
    size_t payloadLen = len - 1;

    // Linear search is O(N), perfectly fast for a small list of OpCodes
    for (size_t i = 0; i < _registryCount; i++) {
        if (_registry[i].opCode == opCode) {
            // Execute the command, passing the payload AND the characteristic
            _registry[i].function(payloadLen, payload, pCharacteristic);
            return;
        }
    }
    
    Serial.printf("Unknown OpCode: 0x%02X\n", opCode);
}