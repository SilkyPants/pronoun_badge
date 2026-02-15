#include "BLEManager.h"
#include <Arduino.h>

void BLEManager::begin(const char* deviceName, const char* serviceUUID) {
    BLEDevice::init(deviceName);
    pServer = BLEDevice::createServer();
    pService = pServer->createService(serviceUUID);
}

void BLEManager::start() {
    if (pService) {
        pService->start();
        pServer->getAdvertising()->start();
        Serial.println("BLE Started Advertising");
    }
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