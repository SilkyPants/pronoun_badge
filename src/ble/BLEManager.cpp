#include "BLEManager.h"
#include <Arduino.h>

void BLEManager::begin(const char* deviceName, const char* serviceUUID) {
    BLEDevice::init(deviceName);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);
    pService = pServer->createService(serviceUUID);
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

void BLEManager::cleanup() {
// 1. Stop advertising and service
    pServer->getAdvertising()->stop();
    pService->stop();

    Serial.println("Cleaning up BLE Callbacks...");
    // 2. Iterate through stored pointers and delete
    for (auto* cb : allocatedCallbacks) {
        delete cb; 
    }
    allocatedCallbacks.clear();
    
    Serial.println("BLE Resources Freed Safely");
}

void BLEManager::addHybridLambda(const char* uuid, 
                     std::function<void(bool)> onSet, 
                     std::function<std::string()> onGet) {
                     
    uint32_t props = BLECharacteristic::PROPERTY_READ | 
                     BLECharacteristic::PROPERTY_WRITE | 
                     BLECharacteristic::PROPERTY_NOTIFY;

    BLECharacteristic* pChar = pService->createCharacteristic(uuid, props);
    pChar->addDescriptor(new BLE2902());

    auto* cb = new LambdaBLECallbacks(onSet, onGet, pChar);
    pChar->setCallbacks(cb);
    allocatedCallbacks.push_back(cb);
}