#pragma once
#include <BLEDevice.h>
#include <BLE2902.h> // REQUIRED FOR NOTIFICATIONS
#include "data_transfer/CommandTypes.h"

class BLEManager : public BLEServerCallbacks, public BLECharacteristicCallbacks
{
public:
    void begin(
        const char *deviceName, 
        const char *serviceUUID, 
        const char* commandCharUUID, 
        const int mtu = 517 /* 517 is the max value */
    );
    void start();
    void loop();
    void cleanup();

private:
    BLEServer *pServer = nullptr;
    BLEService *pService = nullptr;
    BLECharacteristic* pCommandCharacteristic = nullptr;
    bool deviceConnected = false;

    void onConnect(BLEServer *pServer);
    void onDisconnect(BLEServer *pServer);
    void onMtuChanged(BLEServer* pServer, uint16_t mtu);

    // Add these to your private members in BLEManager.h
private:
    const CommandEntry* _registry = nullptr;
    size_t _registryCount = 0;
    BLECharacteristic* pCommandChar = nullptr; // Store this to send notifications back

public:
    void setRegistry(const CommandEntry* registry, size_t count) {
        _registry = registry;
        _registryCount = count;
    }
    
    // Override the onWrite method from BLECharacteristicCallbacks
    void onWrite(BLECharacteristic* pCharacteristic) override;
};

extern BLEManager ble;