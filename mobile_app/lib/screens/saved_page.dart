import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

import '../models/throw_data.dart';
import 'throw_detail_page.dart';

class SavedPage extends StatelessWidget {

  final List<ThrowData> savedThrows;

  final Function(ThrowData) onDelete;

  const SavedPage({
    super.key,
    required this.savedThrows,
    required this.onDelete,
  });

  Future<void> exportCSV() async {

    final csv = StringBuffer(
      "sample_index,throw_id,label,time_ms,"
      "ax,ay,az,gx,gy,gz,mx,my,mz,"
      "accel_mag,gyro_mag,wobble\n",
    );

    for (var t in savedThrows) {
      for (final sample in t.samples) {
        csv.writeln(
          "${sample.sampleIndex},"
          "${sample.throwId},"
          "${sample.label},"
          "${sample.timeMs},"
          "${sample.ax},"
          "${sample.ay},"
          "${sample.az},"
          "${sample.gx},"
          "${sample.gy},"
          "${sample.gz},"
          "${sample.mx},"
          "${sample.my},"
          "${sample.mz},"
          "${sample.accelMag},"
          "${sample.gyroMag},"
          "${t.wobble}",
        );
      }
    }

    final dir =
        await getTemporaryDirectory();

    final file =
        File("${dir.path}/throw_samples.csv");

    await file.writeAsString(csv.toString());

    await SharePlus.instance.share(
      ShareParams(
        files: [XFile(file.path)],
        text: "Frisbee throw dataset",
      ),
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
              onTap: () {
                Navigator.of(context).push(
                  MaterialPageRoute(
                    builder: (_) => ThrowDetailPage(
                      throwData: throwData,
                    ),
                  ),
                );
              },

              title: Text(throwData.label),

              subtitle: Text(
                "Flight Time: "
                "${throwData.flightTime.toStringAsFixed(2)} s\n"
                "Max Accel: "
                "${throwData.maxAccel.toStringAsFixed(2)} g | "
                "Max Gyro: "
                "${throwData.maxGyro.toStringAsFixed(0)}\n"
                "Wobbly: "
                "${throwData.wobble ? "Yes" : "No"}",
              ),
              isThreeLine: true,

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
