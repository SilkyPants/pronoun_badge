#include "DataTransferManager.h"

#include <BLEDevice.h>
#include <esp_rom_crc.h>

TransferMode DataTransferManager::_mode = TransferMode::NONE;

File DataTransferManager::_downloadFile;
File DataTransferManager::_fsFile;
uint32_t DataTransferManager::_expectedSize = 0;
uint32_t DataTransferManager::_bytesReceived = 0;
uint32_t DataTransferManager::_currentCrc = 0;
uint8_t DataTransferManager::_lastPercentage = 0;

void DataTransferManager::reset() {
    if (_fsFile) _fsFile.close();
    _mode = TransferMode::NONE;
    _bytesReceived = 0;
    _currentCrc = 0;
    _lastPercentage = 0;
    _expectedSize = 0;
}

void DataTransferManager::handleStartFile(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    if (len < 5) return; // [Size(4)][Name(1+)]
    reset();

    memcpy(&_expectedSize, data, 4);
    char fileName[32];
    size_t nameLen = std::min((size_t)len - 4, (size_t)31);
    memcpy(fileName, &data[4], nameLen);
    fileName[nameLen] = '\0';

    _fsFile = LittleFS.open(fileName[0] == '/' ? fileName : ("/" + String(fileName)).c_str(), FILE_WRITE);
    if (_fsFile) {
        _mode = TransferMode::FILE_UPLOAD;
        sendAck(OpCodes::FILE_UPLOAD_START, true, pChar);
    } else {
        sendAck(OpCodes::FILE_UPLOAD_START, false, pChar);
    }
}

void DataTransferManager::handleStartOTA(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    if (len < 4) return;
    reset();

    memcpy(&_expectedSize, data, 4);
    if (Update.begin(_expectedSize)) {
        _mode = TransferMode::OTA;
        sendAck(OpCodes::OTA_START, true, pChar);
    } else {
        sendAck(OpCodes::OTA_START, false, pChar);
    }
}

void DataTransferManager::handleDownloadReq(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    reset(); // Clear any previous state
    
    char fileName[32];
    size_t nameLen = std::min(len, (size_t)31);
    memcpy(fileName, data, nameLen);
    fileName[nameLen] = '\0';

    _downloadFile = LittleFS.open(fileName[0] == '/' ? fileName : ("/" + String(fileName)).c_str(), "r");

    if (_downloadFile) {
        _mode = TransferMode::FILE_DOWNLOAD;
        _expectedSize = _downloadFile.size();
        _currentCrc = 0;
        
        // Notify Flutter: [DOWNLOAD_START, Success(1), TotalSize(4)]
        uint8_t header[6];
        header[0] = OpCodes::DOWNLOAD_START;
        header[1] = 0x01;
        memcpy(&header[2], &_expectedSize, 4);
        pChar->setValue(header, 6);
        pChar->notify();
    } else {
        sendAck(OpCodes::DOWNLOAD_START, false, pChar);
    }
}

void DataTransferManager::loop(BLECharacteristic* pChar) {
    if (_mode != TransferMode::FILE_DOWNLOAD || !_downloadFile) return;

    // MTU-friendly chunk size (Leave room for 1 byte OpCode)
    uint8_t chunk[500]; 
    chunk[0] = OpCodes::DOWNLOAD_DATA;
    
    size_t bytesRead = _downloadFile.read(&chunk[1], 499);

    if (bytesRead > 0) {
        _currentCrc = esp_rom_crc32_le(_currentCrc, &chunk[1], bytesRead);
        _bytesReceived += bytesRead; // Reusing this var as 'bytesSent'
        
        pChar->setValue(chunk, bytesRead + 1);
        pChar->notify();
        
        // Small delay to prevent congesting the BLE buffer
        // In a high-speed app, you'd check pChar->getProperties() & notify bit
        delay(5); 
    } else {
        // End of file
        uint8_t endPacket[5];
        endPacket[0] = OpCodes::DOWNLOAD_END;
        memcpy(&endPacket[1], &_currentCrc, 4);
        
        pChar->setValue(endPacket, 5);
        pChar->notify();
        
        reset(); // Closes file and sets mode to NONE
        Serial.println("Download complete");
    }
}

void DataTransferManager::handleData(size_t len, const uint8_t* data, BLECharacteristic* pChar) {

    if (_mode == TransferMode::NONE || _expectedSize == 0) return;

    // 1. Update CRC and write data
    _currentCrc = esp_rom_crc32_le(_currentCrc, data, len);
    _bytesReceived += len;

    if (_mode == TransferMode::FILE_UPLOAD) _fsFile.write(data, len);
    else if (_mode == TransferMode::OTA) Update.write((uint8_t*)data, len);

    // 2. Calculate Progress
    uint8_t currentPercentage = (uint8_t)((_bytesReceived * 100) / _expectedSize);

    // 3. Only notify if the percentage has actually increased
    if (currentPercentage > _lastPercentage) {
        _lastPercentage = currentPercentage;
        
        uint8_t progress[] = { OpCodes::UPLOAD_PROGRESS, _lastPercentage };
        pChar->setValue(progress, 2);
        pChar->notify();
    }
}

void DataTransferManager::handleEnd(size_t len, const uint8_t* data, BLECharacteristic* pChar) {
    if (len < 4 || _mode == TransferMode::NONE) return;

    uint32_t remoteCrc;
    memcpy(&remoteCrc, data, 4);

    bool success = (remoteCrc == _currentCrc) && (_bytesReceived == _expectedSize);

    if (success) {
        if (_mode == TransferMode::OTA) {
            success = Update.end(true);
            sendAck(OpCodes::UPLOAD_END, success, pChar);
            if (success) {
                delay(500);
                esp_restart();
            }
        } else {
            _fsFile.close();
            sendAck(OpCodes::UPLOAD_END, true, pChar);
        }
    } else {
        if (_mode == TransferMode::OTA) Update.abort();
        sendAck(OpCodes::UPLOAD_END, false, pChar);
    }
    reset();
}

void DataTransferManager::sendAck(uint8_t opCode, bool success, BLECharacteristic* pChar) {
    uint8_t res[] = { opCode, success ? OpCodes::STATUS_OK : OpCodes::STATUS_ERR };
    pChar->setValue(res, 2);
    pChar->notify();
}