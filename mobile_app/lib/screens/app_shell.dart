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
  ];

  String selectedThrowType = "Backhand";

  bool sameUploadedThrow(ThrowData a, ThrowData b) {
    return a.samples.isNotEmpty &&
        b.samples.isNotEmpty &&
        a.throwId == b.throwId &&
        a.label == b.label &&
        a.samples.length == b.samples.length &&
        (a.flightTime - b.flightTime).abs() < 0.001;
  }

  void addSavedThrow(ThrowData throwData) {
    savedThrows.removeWhere(
      (savedThrow) => sameUploadedThrow(savedThrow, throwData),
    );
    savedThrows.insert(0, throwData);
  }

  void saveThrow(ThrowData throwData) {

    setState(() {

      liveThrows.remove(throwData);

      addSavedThrow(throwData);
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

      liveThrows.removeWhere(
        (liveThrow) => sameUploadedThrow(liveThrow, throwData),
      );
      liveThrows.insert(0, throwData);
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
            addSavedThrow(throwData);
          });
        },
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
            addSavedThrow(throwData);
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
