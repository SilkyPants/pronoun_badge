import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:glowbug/opcodes.dart';
import 'package:provider/provider.dart';

import 'common.dart';

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
      serviceId: targetServiceUuid,
      characteristicId:
          commandCharacteristicUuid, // Use your WRITE characteristic here
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
      serviceId: targetServiceUuid,
      characteristicId: commandCharacteristicUuid,
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

                case OpCodes.restartEsp:
                  debugPrint("Restart OpCode ACK");
                  Navigator.pop(context);
                  break;

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

  Future<void> _restartESP() async {
    final ble = Provider.of<FlutterReactiveBle>(context, listen: false);

    final characteristic = QualifiedCharacteristic(
      serviceId: targetServiceUuid,
      characteristicId: commandCharacteristicUuid,
      deviceId: widget.deviceId,
    );

    try {
      // PACKET: [OpCode, Payload]
      await ble.writeCharacteristicWithResponse(
        characteristic,
        value: OpCodes.commandWithByte(OpCodes.restartEsp, 0),
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

  Future<void> _toggleFlash() async {
    final ble = Provider.of<FlutterReactiveBle>(context, listen: false);

    // LOGIC:
    // If text is "Blinking", we want to stop it -> Write 0
    // If text is "Stopped" (or anything else), we want to start it -> Write 1
    final int stateValue = _isCurrentlyBlinking ? 0 : 1;

    final characteristic = QualifiedCharacteristic(
      serviceId: targetServiceUuid,
      characteristicId: commandCharacteristicUuid,
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

                  SizedBox.square(dimension: 16),

                  // --- RESTART BUTTON ---
                  ElevatedButton.icon(
                    onPressed: _isReady ? _restartESP : null,
                    icon: Icon(Icons.lock_reset),
                    label: Text("RESTART"),
                    style: ElevatedButton.styleFrom(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 30,
                        vertical: 15,
                      ),
                      backgroundColor: Colors.red,
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
