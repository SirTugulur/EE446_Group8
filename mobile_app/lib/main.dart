import 'package:flutter/material.dart';

import 'screens/app_shell.dart';

void main() {

  runApp(const FrisbeeApp());
}

class FrisbeeApp extends StatelessWidget {

  const FrisbeeApp({super.key});

  @override
  Widget build(BuildContext context) {

    return MaterialApp(

      debugShowCheckedModeBanner: false,

      theme: ThemeData.dark(),

      home: const AppShell(),
    );
  }
}