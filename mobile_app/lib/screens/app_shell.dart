import 'package:flutter/material.dart';

import '../models/throw_data.dart';

import 'classification_page.dart';
import 'data_collection_page.dart';

class AppShell extends StatefulWidget {
  final bool enableBluetoothStartup;

  const AppShell({
    super.key,
    this.enableBluetoothStartup = true,
  });

  @override
  State<AppShell> createState() => _AppShellState();
}

class _AppShellState extends State<AppShell> {

  int currentIndex = 0;

  final List<ThrowData> liveThrows = [];
  final List<ThrowData> savedThrows = [];

  final List<String> throwTypes = [
    "Backhand",
    "Forehand",
    "Hammer",
    "Scoober",
    "Blade",
  ];

  String selectedThrowType = "Backhand";

  void saveThrow(ThrowData throwData) {

    setState(() {

      liveThrows.remove(throwData);

      savedThrows.add(throwData);
    });
  }

  void deleteLiveThrow(ThrowData throwData) {

    setState(() {

      liveThrows.remove(throwData);
    });
  }

  void deleteSavedThrow(ThrowData throwData) {

    setState(() {

      savedThrows.remove(throwData);
    });
  }

  void addThrow(ThrowData throwData) {

    setState(() {

      liveThrows.insert(0, throwData);
    });
  }

  void updateWobble(
      ThrowData throwData,
      bool wobble) {

    setState(() {

      throwData.wobble = wobble;
    });
  }

  @override
  Widget build(BuildContext context) {

    final pages = [

      DataCollectionPage(
        enableBluetoothStartup: widget.enableBluetoothStartup,
        liveThrows: liveThrows,
        savedThrows: savedThrows,
        throwTypes: throwTypes,
        selectedThrowType: selectedThrowType,
        onSave: saveThrow,
        onDeleteLive: deleteLiveThrow,
        onDeleteSaved: deleteSavedThrow,
        onAddThrow: addThrow,
        onClassifiedThrow: (throwData) {
          setState(() {
            savedThrows.insert(0, throwData);
          });
        },
        onWobbleChanged: updateWobble,
        onThrowTypeChanged: (value) {

          setState(() {

            selectedThrowType = value!;
          });
        },
      ),

      ClassificationPage(
        enableBluetoothStartup: widget.enableBluetoothStartup,
        classifiedThrows: savedThrows,
        throwTypes: throwTypes,
        selectedThrowType: selectedThrowType,
        onClassifiedThrow: (throwData) {
          setState(() {
            savedThrows.insert(0, throwData);
          });
        },
        onThrowTypeChanged: (value) {
          setState(() {
            selectedThrowType = value!;
          });
        },
      ),
    ];

    return Scaffold(

      body: pages[currentIndex],

      bottomNavigationBar: BottomNavigationBar(

        currentIndex: currentIndex,

        onTap: (index) {

          setState(() {

            currentIndex = index;
          });
        },

        items: const [

          BottomNavigationBarItem(
            icon: Icon(Icons.sensors),
            label: "Data",
          ),

          BottomNavigationBarItem(
            icon: Icon(Icons.analytics),
            label: "Classify",
          ),
        ],
      ),
    );
  }
}
