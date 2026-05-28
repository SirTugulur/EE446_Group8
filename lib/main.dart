import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  print(License.values);

  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Frisbee Tracker',
      theme: ThemeData.dark(),
      home: const BLEPage(),
    );
  }
}

class BLEPage extends StatefulWidget {
  const BLEPage({super.key});

  @override
  State<BLEPage> createState() => _BLEPageState();
}

class _BLEPageState extends State<BLEPage> {

  List<ScanResult> scanResults = [];

  BluetoothDevice? connectedDevice;

  @override
  void initState() {
    super.initState();

    scanForDevices();
  }

  Future<void> scanForDevices() async {

    scanResults.clear();

    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.location.request();

    // Stop old scan first
    await FlutterBluePlus.stopScan();

    // Listen for results
    FlutterBluePlus.scanResults.listen((results) {

      setState(() {
        scanResults = results;
      });

      for (var r in results) {

        print("FOUND DEVICE:");
        print("Name: ${r.device.platformName}");
        print("Adv Name: ${r.advertisementData.advName}");
        print("ID: ${r.device.remoteId}");
        print("-------------------");
      }
    });

    // Start scan
    await FlutterBluePlus.startScan(
      timeout: const Duration(seconds: 5),
    );
  }

  Future<void> connectToDevice(BluetoothDevice device) async {

    try {

      await FlutterBluePlus.stopScan();

      print("CONNECTING...");

      await device.connect(
        license: License.free,
      );

      connectedDevice = device;

      print("CONNECTED TO:");
      print(device.platformName);

      // Discover services
      List<BluetoothService> services =
          await device.discoverServices();

      print("SERVICES FOUND:");

      for (var service in services) {

        print(service.uuid);

        for (var characteristic in service.characteristics) {

          print("  CHARACTERISTIC:");
          print("  ${characteristic.uuid}");
        }
      }

      setState(() {});

    } catch (e) {

      print("CONNECTION ERROR:");
      print(e);
    }
  }

  @override
  Widget build(BuildContext context) {

    return Scaffold(

      appBar: AppBar(
        title: const Text("Frisbee Tracker"),
      ),

      body: scanResults.isEmpty
    ? const Center(
        child: Text(
          "No BLE devices found",
          style: TextStyle(fontSize: 22),
        ),
      )
    : ListView.builder(
        itemCount: scanResults.length,

        itemBuilder: (context, index) {

          final result = scanResults[index];

          return ListTile(

            title: Text(
              result.device.platformName.isEmpty
                  ? "Unknown Device"
                  : result.device.platformName,
            ),

            subtitle: Text(result.device.remoteId.toString()),

            trailing: ElevatedButton(
              child: const Text("Connect"),

              onPressed: () async {

                try {

                  print("CONNECTING...");

                  await result.device.connect(
                    license: License.free,
                  );

                  print("CONNECTED!");

                } catch (e) {

                  print("ERROR:");
                  print(e);
                }
              },
            ),
          );
        },
      ),

      floatingActionButton: FloatingActionButton(
        onPressed: scanForDevices,
        child: const Icon(Icons.refresh),
      ),
    );
  }
}