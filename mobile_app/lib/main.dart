import 'package:flutter/material.dart';

import 'screens/app_shell.dart';

void main() {
  runApp(const FrisbeeApp());
}

class FrisbeeApp extends StatelessWidget {
  final bool enableBluetoothStartup;

  const FrisbeeApp({
    super.key,
    this.enableBluetoothStartup = true,
  });

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark(),
      home: AppShell(enableBluetoothStartup: enableBluetoothStartup),
    );
  }
}
