import 'dart:async';
import 'dart:io'; // Needed for Platform check
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

/// Single Source of Truth for BLE Binary Protocol.
/// Matches the C++ OpCodes struct in Protocol.h
abstract class OpCodes {
  // --- Commands (Flutter -> ESP32) ---
  static const int setLed = 0x01;
  static const int getStatus = 0x02;
  static const int listFiles = 0x04;

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

// --- CONFIGURATION UUIDS ---
final Uuid _targetServiceUuid = Uuid.parse(
  "4fafc201-1fb5-459e-8fcc-c5c9c331914b",
);
final Uuid _commandCharacteristicUuid = Uuid.parse(
  "beb5483e-36e1-4688-b7f5-ea07361b26a8",
);

void main() {
  runApp(
    Provider<FlutterReactiveBle>(
      create: (_) => FlutterReactiveBle(),
      child: const MaterialApp(home: ScanningScreen()),
    ),
  );
}

// --- SCANNING SCREEN ---
class ScanningScreen extends StatefulWidget {
  const ScanningScreen({super.key});

  @override
  State<ScanningScreen> createState() => _ScanningScreenState();
}

class _ScanningScreenState extends State<ScanningScreen> {
  final List<DiscoveredDevice> _devices = [];
  StreamSubscription? _scanSub;
  bool _isScanning = false;
  bool _permissionsGranted = false;

  @override
  void initState() {
    super.initState();
    _requestPermissions();
  }

  Future<void> _requestPermissions() async {
    // Android 12+ requires explicit Scan/Connect permissions.
    // Android <12 requires Location permission for BLE scanning.
    // iOS requires Bluetooth permission (handled automatically by Info.plist usually, but good to check).

    if (Platform.isAndroid) {
      Map<Permission, PermissionStatus> statuses = await [
        Permission.location,
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
      ].request();

      // Check if all requested permissions are granted
      bool allGranted = statuses.values.every((status) => status.isGranted);

      if (mounted) {
        setState(() => _permissionsGranted = allGranted);
      }
    } else {
      // For iOS, the OS handles the dialog on first use of the BLE feature,
      // but we can request explicitly to be safe.
      if (await Permission.bluetooth.request().isGranted) {
        if (mounted) setState(() => _permissionsGranted = true);
      }
    }
  }

  void _toggleScan(FlutterReactiveBle ble) {
    if (!_permissionsGranted) {
      _requestPermissions();
      return;
    }

    if (_isScanning) {
      _scanSub?.cancel();
      setState(() => _isScanning = false);
    } else {
      _devices.clear();
      setState(() => _isScanning = true);

      _scanSub = ble
          .scanForDevices(withServices: [_targetServiceUuid])
          .listen(
            (device) {
              final index = _devices.indexWhere((d) => d.id == device.id);
              if (index >= 0) {
                setState(() => _devices[index] = device);
              } else {
                setState(() => _devices.add(device));
              }
            },
            onError: (e) {
              debugPrint("Scan Error: $e");
            },
          );
    }
  }

  @override
  Widget build(BuildContext context) {
    final ble = Provider.of<FlutterReactiveBle>(context);

    return Scaffold(
      appBar: AppBar(title: const Text("Badge Scanner")),
      body: Column(
        children: [
          // Visual Warning if permissions are missing
          if (!_permissionsGranted)
            Container(
              color: Colors.red.shade100,
              padding: const EdgeInsets.all(8),
              width: double.infinity,
              child: const Text(
                "Bluetooth/Location permissions are required to scan.",
                textAlign: TextAlign.center,
                style: TextStyle(color: Colors.red),
              ),
            ),

          Padding(
            padding: const EdgeInsets.all(16.0),
            child: ElevatedButton.icon(
              icon: Icon(
                _permissionsGranted
                    ? (_isScanning ? Icons.stop : Icons.play_arrow)
                    : Icons.lock,
              ),
              onPressed: () => _toggleScan(ble),
              label: Text(
                _permissionsGranted
                    ? (_isScanning ? "Stop Scan" : "Start Scan")
                    : "Grant Permissions",
              ),
              style: ElevatedButton.styleFrom(
                backgroundColor: _permissionsGranted
                    ? Colors.blue
                    : Colors.grey,
                foregroundColor: Colors.white,
              ),
            ),
          ),

          Expanded(
            child: ListView.builder(
              itemCount: _devices.length,
              itemBuilder: (context, i) {
                final d = _devices[i];
                return ListTile(
                  title: Text(d.name.isEmpty ? "Unknown Badge" : d.name),
                  subtitle: Text("${d.id}\nRSSI: ${d.rssi}"),
                  trailing: const Icon(Icons.bluetooth_audio),
                  onTap: () {
                    _scanSub?.cancel();
                    setState(() => _isScanning = false);
                    Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (_) => ConnectedPage(deviceId: d.id),
                      ),
                    );
                  },
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

// --- CONNECTED SCREEN (Unchanged from previous logic) ---
class ConnectedPage extends StatefulWidget {
  final String deviceId;
  const ConnectedPage({super.key, required this.deviceId});

  @override
  State<ConnectedPage> createState() => _ConnectedPageState();
}

class _ConnectedPageState extends State<ConnectedPage> {
  StreamSubscription<ConnectionStateUpdate>? _connectionSub;
  StreamSubscription<List<int>>? _notifySub;

  String _statusText = "Waiting..."; // Stores "Stopped" or "Blinking"
  bool _isCurrentlyBlinking = false;
  bool _isConnected = false;
  bool _isReady = false;

  @override
  void initState() {
    super.initState();
    _connect();
  }

  void _connect() {
    final ble = Provider.of<FlutterReactiveBle>(context, listen: false);
    _connectionSub = ble.connectToDevice(id: widget.deviceId).listen((
      update,
    ) async {
      debugPrint("Connection State: ${update.connectionState}");
      if (update.connectionState == DeviceConnectionState.connected) {
        if (!_isConnected) {
          setState(() => _isConnected = true);
          _subscribeToNotifications(ble);
          // Small delay to let connection settle
          await Future.delayed(const Duration(milliseconds: 200));
          await _readInitialState(ble);
        }
      } else if (update.connectionState == DeviceConnectionState.disconnected) {
        if (mounted) setState(() => _isConnected = false);
      }
    }, onError: (Object e) => debugPrint("Connection Error: $e"));
  }

  Future<void> _readInitialState(FlutterReactiveBle ble) async {
    final characteristic = QualifiedCharacteristic(
      serviceId: _targetServiceUuid,
      characteristicId:
          _commandCharacteristicUuid, // Use your WRITE characteristic here
      deviceId: widget.deviceId,
    );

    try {
      debugPrint("Requesting initial state...");

      await ble.writeCharacteristicWithResponse(
        characteristic,
        value: [OpCodes.getStatus],
      );

      if (mounted) setState(() => _isReady = true);
    } catch (e) {
      debugPrint("Request State Error: $e");
      if (mounted) setState(() => _isReady = true);
    }
  }

  void updateState(bool newState) {
    _isCurrentlyBlinking = newState;
    _statusText = _isCurrentlyBlinking ? "Blinking" : "Stopped";
  }

  void _subscribeToNotifications(FlutterReactiveBle ble) {
    final characteristic = QualifiedCharacteristic(
      serviceId: _targetServiceUuid,
      characteristicId: _commandCharacteristicUuid,
      deviceId: widget.deviceId,
    );

    _notifySub = ble
        .subscribeToCharacteristic(characteristic)
        .listen(
          (data) {
            if (data.isEmpty) return;

            final int opCode = data[0];
            debugPrint(
              "Notify OpCode: 0x${opCode.toRadixString(16).padLeft(2, '0')} Raw: $data",
            );

            if (!mounted) return;

            setState(() {
              switch (opCode) {
                case OpCodes.setLed:
                case OpCodes.getStatus:
                  // Both of these contain LED state in index 1
                  updateState(data[1] == 1);
                  // If it's the full status packet, also update FS stubs
                  // if (data.length >= 10) {
                  //   final bd = ByteData.sublistView(Uint8List.fromList(data));
                  //   // _fsTotalBytes = bd.getUint32(2, Endian.little);
                  //   // _fsUsedBytes = bd.getUint32(6, Endian.little);
                  // }
                  break;

                // case OpCodes.uploadProgress:
                //   _uploadProgress = data[1] / 100.0;
                //   debugPrint("Upload Progress: ${data[1]}%");
                //   break;

                // case OpCodes.fileEntry:
                //   // Handle directory listing stream
                //   String fileName = String.fromCharCodes(data.sublist(1));
                //   _discoveredFiles.add(fileName);
                //   break;

                // case OpCodes.uploadEnd:
                //   bool success = data[1] == OpCodes.statusOk;
                //   _isUploading = false;
                //   _uploadProgress = 0;
                //   ScaffoldMessenger.of(context).showSnackBar(
                //     SnackBar(
                //       content: Text(
                //         success
                //             ? "Transfer Successful"
                //             : "Transfer Failed (CRC Mismatch)",
                //       ),
                //     ),
                //   );
                //   break;

                default:
                  debugPrint("Unhandled OpCode: $opCode");
              }
            });
          },
          onError: (dynamic error) {
            debugPrint("Notification Error: $error");
          },
        );
  }

  Future<void> _toggleFlash() async {
    final ble = Provider.of<FlutterReactiveBle>(context, listen: false);

    // LOGIC:
    // If text is "Blinking", we want to stop it -> Write 0
    // If text is "Stopped" (or anything else), we want to start it -> Write 1
    final int stateValue = _isCurrentlyBlinking ? 0 : 1;

    final characteristic = QualifiedCharacteristic(
      serviceId: _targetServiceUuid,
      characteristicId: _commandCharacteristicUuid,
      deviceId: widget.deviceId,
    );

    try {
      // PACKET: [OpCode, Payload]
      // We send OpCodes.setLed (0x01) followed by the state (0 or 1)
      await ble.writeCharacteristicWithResponse(
        characteristic,
        value: OpCodes.commandWithByte(OpCodes.setLed, stateValue),
      );
      // We wait for the notification to update the UI text
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text("Write Failed: $e")));
      }
    }
  }

  @override
  void dispose() {
    _notifySub?.cancel();
    _connectionSub?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    // Determine UI state based on the string

    return Scaffold(
      appBar: AppBar(title: const Text("Badge Control")),
      body: Center(
        child: !_isConnected
            ? const Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  CircularProgressIndicator(),
                  SizedBox(height: 16),
                  Text("Connecting..."),
                ],
              )
            : Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  // --- STATUS DISPLAY ---
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 40,
                      vertical: 20,
                    ),
                    decoration: BoxDecoration(
                      color: _isCurrentlyBlinking
                          ? Colors.green.shade100
                          : Colors.red.shade100,
                      borderRadius: BorderRadius.circular(15),
                      border: Border.all(
                        color: _isCurrentlyBlinking ? Colors.green : Colors.red,
                        width: 2,
                      ),
                    ),
                    child: Column(
                      children: [
                        Text(
                          _statusText.toUpperCase(),
                          style: TextStyle(
                            fontSize: 32,
                            fontWeight: FontWeight.bold,
                            color: _isCurrentlyBlinking
                                ? Colors.green.shade800
                                : Colors.red.shade800,
                          ),
                        ),
                        const SizedBox(height: 5),
                        const Text(
                          "Device Status",
                          style: TextStyle(fontSize: 12, color: Colors.grey),
                        ),
                      ],
                    ),
                  ),

                  const SizedBox(height: 50),

                  // --- TOGGLE BUTTON ---
                  ElevatedButton.icon(
                    onPressed: _isReady ? _toggleFlash : null,
                    icon: Icon(
                      _isCurrentlyBlinking ? Icons.flash_off : Icons.flash_on,
                    ),
                    label: Text(
                      _isCurrentlyBlinking ? "STOP FLASHING" : "START FLASHING",
                    ),
                    style: ElevatedButton.styleFrom(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 30,
                        vertical: 15,
                      ),
                      backgroundColor: _isCurrentlyBlinking
                          ? Colors.red
                          : Colors.green, // Visual cue
                      foregroundColor: Colors.white,
                    ),
                  ),

                  const SizedBox(height: 40),
                  OutlinedButton(
                    onPressed: () => Navigator.pop(context),
                    child: const Text("Disconnect & Go Back"),
                  ),
                ],
              ),
      ),
    );
  }
}
