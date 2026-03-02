#pragma once
#include <cstdint>
#include <string_view>

class BLECharacteristic; // Forward declaration

// The new signature gives the command direct access to respond via BLE
using CommandFunc = void (*)(size_t dataLen, const uint8_t* data, BLECharacteristic* pChar);

struct CommandEntry {
    uint8_t opCode;
    CommandFunc function;
};