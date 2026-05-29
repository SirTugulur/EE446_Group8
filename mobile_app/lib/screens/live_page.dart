import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import '../models/throw_data.dart';
import '../services/fake_throw_service.dart';

class LivePage extends StatefulWidget {

  final List<ThrowData> liveThrows;

  final Function(ThrowData) onSave;
  final Function(ThrowData) onDelete;
  final Function(ThrowData) onAddThrow;
  final Function(ThrowData, bool) onWobbleChanged;

  final List<String> throwTypes;
  final String selectedThrowType;

  final Function(String?) onThrowTypeChanged;

  const LivePage({
    super.key,
    required this.liveThrows,
    required this.onSave,
    required this.onDelete,
    required this.onAddThrow,
    required this.onWobbleChanged,
    required this.throwTypes,
    required this.selectedThrowType,
    required this.onThrowTypeChanged,
  });

  @override
  State<LivePage> createState() => _LivePageState();
}

class _LivePageState extends State<LivePage> {

  bool isConnected = false;

  BluetoothDevice? connectedDevice;

  List<ScanResult> scanResults = [];

  @override
  void initState() {
    super.initState();

    scanForDevices();
  }

  Future<void> scanForDevices() async {

    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.location.request();

    scanResults.clear();

    setState(() {});

    FlutterBluePlus.startScan(
      timeout: const Duration(seconds: 5),
    );

    FlutterBluePlus.scanResults.listen((results) {

      setState(() {

        scanResults = results;
      });
    });
  }

  Future<void> connectToDevice(
      BluetoothDevice device) async {

    try {

      await device.connect(
        license: License.free,
      );

      connectedDevice = device;

      setState(() {

        isConnected = true;
      });

      List<BluetoothService> services =
          await device.discoverServices();

      debugPrint(
        "Services Found: ${services.length}",
      );

    } catch (e) {

      debugPrint(
        "Connection Error: $e",
      );
    }
  }

  @override
  Widget build(BuildContext context) {

    return Scaffold(

      appBar: AppBar(
        title: const Text("Live Throws"),
      ),

      body: Column(

        children: [

          const SizedBox(height: 10),

          //--------------------------------
          // BLE STATUS
          //--------------------------------

          Row(

            mainAxisAlignment:
                MainAxisAlignment.center,

            children: [

              Icon(
                Icons.circle,
                color: isConnected
                    ? Colors.green
                    : Colors.red,
                size: 14,
              ),

              const SizedBox(width: 8),

              Text(
                isConnected
                    ? "BLE Connected"
                    : "BLE Disconnected",
              ),
            ],
          ),

          const SizedBox(height: 15),

          //--------------------------------
          // SCAN BUTTON
          //--------------------------------

          ElevatedButton(

            onPressed: scanForDevices,

            child: const Text(
              "Scan For Devices",
            ),
          ),

          const SizedBox(height: 10),

          //--------------------------------
          // DEVICE LIST
          //--------------------------------

          SizedBox(

            height: 150,

            child: ListView.builder(

              itemCount: scanResults.length,

              itemBuilder: (context, index) {

                final result =
                    scanResults[index];

                return ListTile(

                  title: Text(
                    result.device.platformName
                            .isEmpty
                        ? "Unknown Device"
                        : result.device.platformName,
                  ),

                  subtitle: Text(
                    result.device.remoteId
                        .toString(),
                  ),

                  trailing: ElevatedButton(

                    onPressed: () {

                      connectToDevice(
                        result.device,
                      );
                    },

                    child:
                        const Text("Connect"),
                  ),
                );
              },
            ),
          ),

          const Divider(),

          //--------------------------------
          // THROW TYPE
          //--------------------------------

          DropdownButton<String>(

            value: widget.selectedThrowType,

            items: widget.throwTypes.map((type) {

              return DropdownMenuItem(

                value: type,

                child: Text(type),
              );
            }).toList(),

            onChanged:
                widget.onThrowTypeChanged,
          ),

          const SizedBox(height: 10),

          //--------------------------------
          // FAKE THROW
          //--------------------------------

          ElevatedButton(

            onPressed: () {

              final throwData =
                  FakeThrowService
                      .generateThrow(
                widget.liveThrows.length + 1,
              );

              throwData.label =
                  widget.selectedThrowType;

              widget.onAddThrow(
                throwData,
              );
            },

            child: const Text(
              "Generate Fake Throw",
            ),
          ),

          const SizedBox(height: 10),

          //--------------------------------
          // THROW QUEUE
          //--------------------------------

          Expanded(

            child: ListView.builder(

              itemCount:
                  widget.liveThrows.length,

              itemBuilder: (
                context,
                index,
              ) {

                final throwData =
                    widget.liveThrows[index];

                return Card(

                  margin:
                      const EdgeInsets.all(8),

                  child: Padding(

                    padding:
                        const EdgeInsets.all(12),

                    child: Column(

                      crossAxisAlignment:
                          CrossAxisAlignment
                              .start,

                      children: [

                        Text(
                          throwData.label,
                          style:
                              const TextStyle(
                            fontSize: 22,
                            fontWeight:
                                FontWeight.bold,
                          ),
                        ),

                        Text(
                          "Flight Time: ${throwData.flightTime.toStringAsFixed(2)} s",
                        ),

                        Text(
                          "Max Accel: ${throwData.maxAccel.toStringAsFixed(2)} g",
                        ),

                        Text(
                          "Max Gyro: ${throwData.maxGyro.toStringAsFixed(0)}",
                        ),

                        CheckboxListTile(

                          value:
                              throwData.wobble,

                          title: const Text(
                            "Wobbly",
                          ),

                          onChanged: (value) {

                            widget
                                .onWobbleChanged(
                              throwData,
                              value ?? false,
                            );
                          },
                        ),

                        Row(

                          children: [

                            ElevatedButton(

                              onPressed: () {

                                widget.onSave(
                                  throwData,
                                );
                              },

                              child:
                                  const Text(
                                "Save",
                              ),
                            ),

                            const SizedBox(
                              width: 8,
                            ),

                            ElevatedButton(

                              onPressed: () {

                                widget.onDelete(
                                  throwData,
                                );
                              },

                              child:
                                  const Text(
                                "Delete",
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}