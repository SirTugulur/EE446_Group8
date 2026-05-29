import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

import '../models/throw_data.dart';

class SavedPage extends StatelessWidget {

  final List<ThrowData> savedThrows;

  final Function(ThrowData) onDelete;

  const SavedPage({
    super.key,
    required this.savedThrows,
    required this.onDelete,
  });

  Future<void> exportCSV() async {

    String csv =
        "throw_id,label,flight_time,max_accel,max_gyro,wobble\n";

    for (var t in savedThrows) {

      csv +=
          "${t.throwId},"
          "${t.label},"
          "${t.flightTime},"
          "${t.maxAccel},"
          "${t.maxGyro},"
          "${t.wobble}\n";
    }

    final dir =
        await getTemporaryDirectory();

    final file =
        File("${dir.path}/throws.csv");

    await file.writeAsString(csv);

    await Share.shareXFiles(
      [XFile(file.path)],
      text: "Frisbee throw dataset",
    );
  }

  @override
  Widget build(BuildContext context) {

    return Scaffold(

      appBar: AppBar(

        title: const Text("Saved Throws"),

        actions: [

          IconButton(
            onPressed: exportCSV,
            icon: const Icon(Icons.share),
          ),
        ],
      ),

      body: ListView.builder(

        itemCount: savedThrows.length,

        itemBuilder: (context, index) {

          final throwData =
              savedThrows[index];

          return Card(

            margin: const EdgeInsets.all(8),

            child: ListTile(

              title: Text(throwData.label),

              subtitle: Text(
                "Flight Time: "
                "${throwData.flightTime.toStringAsFixed(2)} s",
              ),

              trailing: IconButton(

                icon: const Icon(Icons.delete),

                onPressed: () {

                  onDelete(throwData);
                },
              ),
            ),
          );
        },
      ),
    );
  }
}