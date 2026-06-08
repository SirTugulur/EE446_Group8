import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

import '../models/throw_data.dart';
import 'throw_detail_page.dart';

class SavedPage extends StatelessWidget {

  final List<ThrowData> savedThrows;

  final Function(ThrowData) onDelete;
  final bool showAppBar;

  const SavedPage({
    super.key,
    required this.savedThrows,
    required this.onDelete,
    this.showAppBar = true,
  });

  Future<void> exportCSV() async {

    final csv = StringBuffer(
      "sample_index,throw_id,label,time_ms,"
      "ax,ay,az,gx,gy,gz,mx,my,mz,"
      "accel_mag,gyro_mag,wobble,completed\n",
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
          "${t.wobble},"
          "${t.completed ?? ""}",
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
    final throwsByType = <String, List<ThrowData>>{};

    for (final throwData in savedThrows) {
      throwsByType.putIfAbsent(throwData.label, () => []).add(throwData);
    }

    final labels = throwsByType.keys.toList()..sort();

    final body = savedThrows.isEmpty
        ? const Center(child: Text("No saved throws yet"))
        : ListView.builder(
            itemCount: labels.length,
            itemBuilder: (context, index) {
              final label = labels[index];
              final throws = throwsByType[label]!;

              return Card(
                margin: const EdgeInsets.all(8),
                child: ExpansionTile(
                  leading: const Icon(Icons.folder),
                  title: Text(label),
                  subtitle: Text(
                    throws.length == 1 ? "1 throw" : "${throws.length} throws",
                  ),
                  children: throws.map((throwData) {
                    return ListTile(
                      contentPadding: const EdgeInsets.fromLTRB(24, 0, 8, 8),
                      onTap: () {
                        Navigator.of(context).push(
                          MaterialPageRoute(
                            builder: (_) => ThrowDetailPage(
                              throwData: throwData,
                            ),
                          ),
                        );
                      },
                      title: Text("Throw #${throwData.throwId}"),
                      subtitle: Text(
                        "Flight Time: "
                        "${throwData.flightTime.toStringAsFixed(2)} s\n"
                        "Max Accel: "
                        "${throwData.maxAccel.toStringAsFixed(2)} g | "
                        "Max Gyro: "
                        "${throwData.maxGyro.toStringAsFixed(0)}\n"
                        "Wobbly: "
                        "${throwData.wobble ? "Yes" : "No"} | "
                        "Completed: "
                        "${_completionText(throwData.completed)}",
                      ),
                      isThreeLine: true,
                      trailing: IconButton(
                        icon: const Icon(Icons.delete),
                        onPressed: () {
                          onDelete(throwData);
                        },
                      ),
                    );
                  }).toList(),
                ),
              );
            },
          );

    if (!showAppBar) {
      return Column(
        children: [
          Align(
            alignment: Alignment.centerRight,
            child: Padding(
              padding: const EdgeInsets.fromLTRB(8, 8, 8, 0),
              child: TextButton.icon(
                onPressed: exportCSV,
                icon: const Icon(Icons.share),
                label: const Text("Export CSV"),
              ),
            ),
          ),
          Expanded(child: body),
        ],
      );
    }

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
      body: body,
    );
  }

  String _completionText(bool? completed) {
    if (completed == null) {
      return "Unknown";
    }

    return completed ? "Yes" : "No";
  }
}
