#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <Update.h>

#include "Protocol.h"

// Forward Declare as it will be found in other places
class BLECharacteristic;

enum class TransferMode { NONE, FILE_UPLOAD, FILE_DOWNLOAD, OTA };

class DataTransferManager {
public:
    // This is the "plug" for your registry
    static void handleStartFile(size_t len, const uint8_t* data, BLECharacteristic* pChar);
    static void handleStartOTA(size_t len, const uint8_t* data, BLECharacteristic* pChar);
    static void handleDownloadReq(size_t len, const uint8_t* data, BLECharacteristic* pChar);
    static void handleData(size_t len, const uint8_t* data, BLECharacteristic* pChar);
    static void handleEnd(size_t len, const uint8_t* data, BLECharacteristic* pChar);

    static void loop(BLECharacteristic* pChar);

private:
    static TransferMode _mode;
    static File _downloadFile;
    static File _fsFile;
    static uint32_t _expectedSize;
    static uint32_t _bytesReceived;
    static uint32_t _currentCrc;
    static uint8_t _lastPercentage;
    
    static void reset();
    static void sendAck(uint8_t opCode, bool success, BLECharacteristic* pChar);
};