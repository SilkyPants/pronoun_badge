#pragma once
#include <cstdint>

struct OpCodes {
    // Commands sent from Flutter to ESP32
    static constexpr uint8_t SET_FLASH      = 0x01;
    static constexpr uint8_t GET_STATUS   = 0x02;
    static constexpr uint8_t LIST_FILES   = 0x04;

    static constexpr uint8_t RESTART_ESP  = 0x99;

    static constexpr uint8_t FILE_UPLOAD_START = 0x10; // Payload: [size(4), name(...)]
    static constexpr uint8_t OTA_START         = 0x20; // Payload: [size(4)]
    static constexpr uint8_t UPLOAD_DATA       = 0x11; // Payload: [raw bytes]
    static constexpr uint8_t UPLOAD_END        = 0x12; // Payload: [crc32(4)]
    static constexpr uint8_t DOWNLOAD_START    = 0x30; // Payload: [FileName]
    static constexpr uint8_t DOWNLOAD_DATA     = 0x31; // Payload: [Raw Bytes]
    static constexpr uint8_t DOWNLOAD_END      = 0x32; // Payload: [CRC32]

    // Responses sent from ESP32 to Flutter
    static constexpr uint8_t FILE_ENTRY         = 0x05;
    static constexpr uint8_t CMD_RESPONSE       = 0x06; // Generic ACK/NACK
    static constexpr uint8_t UPLOAD_PROGRESS    = 0x13; // Payload: [uint8 percentage]
    
    // Status Flags
    static constexpr uint8_t STATUS_ERR   = 0x00;
    static constexpr uint8_t STATUS_OK    = 0x01;
};