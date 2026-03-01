// Needed for Platform check
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:glowbug/scanning_screen.dart';
import 'package:provider/provider.dart';

void main() {
  runApp(
    Provider<FlutterReactiveBle>(
      create: (_) => FlutterReactiveBle(),
      child: const MaterialApp(home: ScanningScreen()),
    ),
  );
}
