import 'package:flutter/material.dart';

import '../models/throw_data.dart';
import 'live_page.dart';
import 'saved_page.dart';

class DataCollectionPage extends StatelessWidget {
  final bool enableBluetoothStartup;
  final List<ThrowData> liveThrows;
  final List<ThrowData> savedThrows;
  final List<String> throwTypes;
  final String selectedThrowType;
  final Function(ThrowData) onSave;
  final Function(ThrowData) onDeleteLive;
  final Function(ThrowData) onDeleteSaved;
  final Function(ThrowData) onAddThrow;
  final Function(ThrowData) onClassifiedThrow;
  final Function(String?) onThrowTypeChanged;

  const DataCollectionPage({
    super.key,
    required this.enableBluetoothStartup,
    required this.liveThrows,
    required this.savedThrows,
    required this.throwTypes,
    required this.selectedThrowType,
    required this.onSave,
    required this.onDeleteLive,
    required this.onDeleteSaved,
    required this.onAddThrow,
    required this.onClassifiedThrow,
    required this.onThrowTypeChanged,
  });

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 2,
      child: Scaffold(
        appBar: AppBar(
          title: const Text("Data Collection"),
          bottom: const TabBar(
            tabs: [
              Tab(icon: Icon(Icons.sensors), text: "Collect"),
              Tab(icon: Icon(Icons.folder), text: "Saved"),
            ],
          ),
        ),
        body: TabBarView(
          children: [
            LivePage(
              showAppBar: false,
              enableBluetoothStartup: enableBluetoothStartup,
              liveThrows: liveThrows,
              onSave: onSave,
              onDelete: onDeleteLive,
              onAddThrow: onAddThrow,
              onClassifiedThrow: onClassifiedThrow,
              throwTypes: throwTypes,
              selectedThrowType: selectedThrowType,
              onThrowTypeChanged: onThrowTypeChanged,
            ),
            SavedPage(
              showAppBar: false,
              savedThrows: savedThrows,
              onDelete: onDeleteSaved,
            ),
          ],
        ),
      ),
    );
  }
}
