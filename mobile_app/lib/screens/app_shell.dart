import 'package:flutter/material.dart';

import '../models/throw_data.dart';

import 'live_page.dart';
import 'saved_page.dart';

class AppShell extends StatefulWidget {

  const AppShell({super.key});

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

      LivePage(
        liveThrows: liveThrows,
        onSave: saveThrow,
        onDelete: deleteLiveThrow,
        onAddThrow: addThrow,
        onWobbleChanged: updateWobble,
        throwTypes: throwTypes,
        selectedThrowType: selectedThrowType,
        onThrowTypeChanged: (value) {

          setState(() {

            selectedThrowType = value!;
          });
        },
      ),

      SavedPage(
        savedThrows: savedThrows,
        onDelete: deleteSavedThrow,
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
            icon: Icon(Icons.sports),
            label: "Live",
          ),

          BottomNavigationBarItem(
            icon: Icon(Icons.save),
            label: "Saved",
          ),
        ],
      ),
    );
  }
}