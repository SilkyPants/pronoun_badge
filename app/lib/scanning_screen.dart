import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:glowbug/common.dart';
import 'package:glowbug/connected_page.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

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
          .scanForDevices(withServices: [targetServiceUuid])
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
