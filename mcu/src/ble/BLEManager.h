#pragma once
#include <BLEDevice.h>

#include "CommandTypes.h"

#include <BLE2902.h> // REQUIRED FOR NOTIFICATIONS

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::optional<std::string> execute(const std::vector<std::string>& args) = 0;
};

// 2. The template wrapper that "holds" any lambda type
template <typename F>
class LambdaWrapper : public ICommand {
    F func;
public:
    LambdaWrapper(F&& f) : func(std::move(f)) {}
    std::optional<std::string> execute(const std::vector<std::string>& args) override {
        return func(args);
    }
};

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