import 'dart:async';
import 'dart:io'; // Needed for Platform check
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

// --- CONFIGURATION UUIDS ---
final Uuid _targetServiceUuid = Uuid.parse(
  "4fafc201-1fb5-459e-8fcc-c5c9c331914b",
);
final Uuid _writeCharUuid = Uuid.parse("beb5483e-36e1-4688-b7f5-ea07361b26a8");
final Uuid _readNotifyCharUuid = _writeCharUuid;
// Uuid.parse(
//   "a1b2c3d4-e5f6-47a8-b9c0-d1e2f3a4b5c6",
//);

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
      characteristicId: _readNotifyCharUuid,
      deviceId: widget.deviceId,
    );

    try {
      final response = await ble.readCharacteristic(characteristic);
      debugPrint("Read Raw Bytes: $response");
      if (mounted) {
        setState(() {
          updateState(response);
          _isReady = true;
        });
      }
    } catch (e) {
      debugPrint("Read Error: $e");
      if (mounted) setState(() => _isReady = true);
    }
  }

  void updateState(List<int> data) {
    // Ascii for '1' due to how things are set up atm
    _isCurrentlyBlinking = data.firstOrNull == 49;
    _statusText = _isCurrentlyBlinking ? "Blinking" : "Stopped";
  }

  void _subscribeToNotifications(FlutterReactiveBle ble) {
    final characteristic = QualifiedCharacteristic(
      serviceId: _targetServiceUuid,
      characteristicId: _readNotifyCharUuid,
      deviceId: widget.deviceId,
    );

    _notifySub = ble
        .subscribeToCharacteristic(characteristic)
        .listen(
          (data) {
            debugPrint("Notify Raw Bytes: $data");
            if (mounted) {
              setState(() {
                updateState(data);
              });
            }
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
    final int valueToWrite = _isCurrentlyBlinking ? 0 : 1;

    final characteristic = QualifiedCharacteristic(
      serviceId: _targetServiceUuid,
      characteristicId: _writeCharUuid,
      deviceId: widget.deviceId,
    );

    try {
      await ble.writeCharacteristicWithResponse(
        characteristic,
        value: [valueToWrite],
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
