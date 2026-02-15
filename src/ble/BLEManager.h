#pragma once
#include <BLEDevice.h>
#include <BLE2902.h> // REQUIRED FOR NOTIFICATIONS
#include <vector>
#include "SmartBLECallbacks.h"

class BLEManager : public BLEServerCallbacks
{
public:
    void begin(const char *deviceName, const char *serviceUUID);
    void start();
    void cleanup();

    // Upgraded Template: Accepts Properties, Setter, and Getter
    template <typename T, typename DataType>
    BLECharacteristic *addCharacteristic(
        const char *uuid,
        uint32_t properties,
        T *instance,
        void (T::*setter)(DataType),
        DataType (T::*getter)() = nullptr)
    {
        BLECharacteristic *pChar = pService->createCharacteristic(uuid, properties);

        // Attach callbacks if a setter or getter is provided
        if (setter || getter)
        {
            auto *cb = new SmartBLECallbacks<T, DataType>(instance, setter, getter);
            pChar->setCallbacks(cb);
            allocatedCallbacks.push_back(cb);
        }

        // If Notify is enabled, add the required BLE2902 Descriptor
        if (properties & BLECharacteristic::PROPERTY_NOTIFY)
        {
            pChar->addDescriptor(new BLE2902());
        }

        return pChar; // Return pointer so main.cpp can send notifications!
    }

    template <typename DataType>
    void addLambdaCharacteristic(const char *uuid,
                                 std::function<void(DataType)> onSet,
                                 std::function<DataType()> onGet)
    {

        uint32_t props = BLECharacteristic::PROPERTY_READ |
                         BLECharacteristic::PROPERTY_WRITE |
                         BLECharacteristic::PROPERTY_NOTIFY;

        BLECharacteristic *pChar = pService->createCharacteristic(uuid, props);
        pChar->addDescriptor(new BLE2902());

        auto *cb = new SymmetricLambdaCallbacks<DataType>(onSet, onGet, pChar);
        pChar->setCallbacks(cb);
        allocatedCallbacks.push_back(cb);
    }

    void addHybridLambda(const char *uuid, std::function<void(bool)> onSet, std::function<std::string()> onGet);

private:
    BLEServer *pServer = nullptr;
    BLEService *pService = nullptr;
    bool deviceConnected = false;
    std::vector<BLECharacteristicCallbacks *> allocatedCallbacks;


    void onConnect(BLEServer *pServer);
    void onDisconnect(BLEServer *pServer);
};