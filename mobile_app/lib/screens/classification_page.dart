import 'package:flutter/material.dart';

import '../models/throw_data.dart';
import 'live_page.dart';
import 'stats_page.dart';

class ClassificationPage extends StatelessWidget {
  final bool enableBluetoothStartup;
  final List<ThrowData> classifiedThrows;
  final List<String> throwTypes;
  final String selectedThrowType;
  final Function(ThrowData) onClassifiedThrow;
  final Function(String?) onThrowTypeChanged;

  const ClassificationPage({
    super.key,
    required this.enableBluetoothStartup,
    required this.classifiedThrows,
    required this.throwTypes,
    required this.selectedThrowType,
    required this.onClassifiedThrow,
    required this.onThrowTypeChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Throw Classification")),
      body: Column(
        children: [
          LivePage(
            showAppBar: false,
            showCollectionControls: false,
            showPendingThrows: false,
            enableBluetoothStartup: enableBluetoothStartup,
            bleModeCommand: "MODE:CLASSIFY",
            liveThrows: const [],
            onSave: (_) {},
            onDelete: (_) {},
            onAddThrow: onClassifiedThrow,
            onClassifiedThrow: onClassifiedThrow,
            onWobbleChanged: (_, _) {},
            throwTypes: throwTypes,
            selectedThrowType: selectedThrowType,
            onThrowTypeChanged: onThrowTypeChanged,
          ),
          const Divider(height: 1),
          Expanded(
            child: StatsPage(
              showAppBar: false,
              savedThrows: classifiedThrows,
              throwTypes: throwTypes,
            ),
          ),
        ],
      ),
    );
  }
}
