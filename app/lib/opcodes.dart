import 'dart:typed_data';

/// Single Source of Truth for BLE Binary Protocol.
/// Matches the C++ OpCodes struct in Protocol.h
abstract class OpCodes {
  // --- Commands (Flutter -> ESP32) ---
  static const int setLed = 0x01;
  static const int getStatus = 0x02;
  static const int listFiles = 0x04;

  static const int restartEsp = 0x99;

  // --- File/OTA Transfer Commands ---
  static const int fileUploadStart = 0x10;
  static const int uploadData = 0x11;
  static const int uploadEnd = 0x12;
  static const int otaStart = 0x20;

  // --- Responses/Notifications (ESP32 -> Flutter) ---
  static const int fileEntry = 0x05;
  static const int uploadProgress = 0x13;

  // --- Status Flags ---
  static const int statusErr = 0x00;
  static const int statusOk = 0x01;

  /// Helper to wrap a command and a single byte payload
  static Uint8List commandWithByte(int opCode, int value) {
    return Uint8List.fromList([opCode, value]);
  }

  /// Helper to wrap a command and a 32-bit integer (Little Endian)
  /// Useful for StartOTA or StartFileUpload where size is required
  static Uint8List commandWithUint32(int opCode, int value) {
    final bdata = ByteData(5);
    bdata.setUint8(0, opCode);
    bdata.setUint32(1, value, Endian.little);
    return bdata.buffer.asUint8List();
  }
}
