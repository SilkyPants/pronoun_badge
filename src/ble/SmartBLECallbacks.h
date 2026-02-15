#pragma once
#include <BLEDevice.h>
#include <vector>
#include <string>
#include <type_traits>

template <typename T, typename DataType>
class SmartBLECallbacks : public BLECharacteristicCallbacks {
public:
    using SetterFn = void (T::*)(DataType);
    using GetterFn = DataType (T::*)();

    // Pass the characteristic pointer so the class can notify itself
    SmartBLECallbacks(T *instance, SetterFn setter, GetterFn getter, BLECharacteristic* pChar)
        : _instance(instance), _setter(setter), _getter(getter), _pChar(pChar) {}

    void onWrite(BLECharacteristic *pCharacteristic) override {
        if (!_instance || !_setter) return;

        uint8_t *data = pCharacteristic->getData();
        size_t len = pCharacteristic->getLength();
        if (!data || len == 0) return;

        // 1. Process the write to your hardware class
        if constexpr (std::is_same<DataType, std::vector<uint8_t>>::value) {
            (_instance->*_setter)({data, data + len});
        }
        else if constexpr (std::is_same<DataType, std::string>::value) {
            (_instance->*_setter)(std::string((char *)data, len));
        }
        else if constexpr (std::is_same<DataType, int>::value) {
            (_instance->*_setter)(atoi((char *)data));
        }

        // 2. AUTO-NOTIFY: Immediately push the updated state back to the phone
        syncAndNotify();
    }

    void onRead(BLECharacteristic *pCharacteristic) override {
        // Just ensures the buffer is fresh before the stack sends it
        updateBuffer();
    }

    // This is the "Magic" function that keeps the phone in sync
    void syncAndNotify() {
        if (updateBuffer() && _pChar) {
            _pChar->notify();
        }
    }

private:
    bool updateBuffer() {
        if (!_instance || !_getter || !_pChar) return false;

        DataType val = (_instance->*_getter)();

        if constexpr (std::is_same<DataType, std::vector<uint8_t>>::value) {
            _pChar->setValue(val.data(), val.size());
        } 
        else if constexpr (std::is_same<DataType, std::string>::value) {
            _pChar->setValue(val);
        } 
        else if constexpr (std::is_same<DataType, int>::value) {
            _pChar->setValue(std::to_string(val));
        }
        return true;
    }

    T *_instance;
    SetterFn _setter;
    GetterFn _getter;
    BLECharacteristic* _pChar;
};

class LambdaBLECallbacks : public BLECharacteristicCallbacks {
public:
    using WriteAction = std::function<void(bool)>;
    using ReadAction = std::function<std::string()>;

    // We add pChar to the constructor so the class knows who to notify
    LambdaBLECallbacks(WriteAction w, ReadAction r, BLECharacteristic* pChar) 
        : _write(w), _read(r), _pChar(pChar) {}

    void onWrite(BLECharacteristic *p) override {
        if (_write) {
            std::string val = p->getValue();
            bool state = (val == "1" || val == "true" || (!val.empty() && val[0] == 0x01));
            _write(state);
            
            // After writing, we usually want to trigger a notify 
            // so the phone UI updates to show the new status string
            triggerNotify();
        }
    }

    void onRead(BLECharacteristic *p) override {
        // onRead is triggered when the phone asks for data.
        // We update the value, but notify isn't strictly required here 
        // since the phone is already receiving the response to its read.
        if (_read) {
            p->setValue(_read());
        }
    }

    // Helper to manually trigger a push from the ESP32
    void triggerNotify() {
        if (_read && _pChar) {
            _pChar->setValue(_read());
            _pChar->notify();
        }
    }

private:
    WriteAction _write;
    ReadAction _read;
    BLECharacteristic* _pChar; // Stored reference
};

template <typename DataType>
class SymmetricLambdaCallbacks : public BLECharacteristicCallbacks {
public:
    using SetterFn = std::function<void(DataType)>;
    using GetterFn = std::function<DataType()>;

    SymmetricLambdaCallbacks(SetterFn s, GetterFn g, BLECharacteristic* pChar) 
        : _setter(s), _getter(g), _pChar(pChar) {}

    void onWrite(BLECharacteristic* p) override {
        if (!_setter) return;
        
        // Use the same parsing logic we built earlier
        DataType value;
        if constexpr (std::is_same_v<DataType, bool>) {
            std::string s = p->getValue();
            value = (s == "1" || s == "true");
        } else if constexpr (std::is_same_v<DataType, int>) {
            value = atoi(p->getValue().c_str());
        } else if constexpr (std::is_same_v<DataType, std::string>) {
            value = p->getValue();
        }

        _setter(value);

        // AUTO-NOTIFY: Update the internal value and push to clients
        syncAndNotify();
    }

    void onRead(BLECharacteristic* p) override {
        if (_getter) {
            syncAndNotify(); // Ensure the read gets the absolute latest state
        }
    }

    void syncAndNotify() {
        if (_getter && _pChar) {
            DataType currentVal = _getter();
            if constexpr (std::is_same_v<DataType, std::string>) {
                _pChar->setValue(currentVal);
            } else {
                _pChar->setValue(std::to_string(currentVal));
            }
            _pChar->notify();
        }
    }

private:
    SetterFn _setter;
    GetterFn _getter;
    BLECharacteristic* _pChar;
};