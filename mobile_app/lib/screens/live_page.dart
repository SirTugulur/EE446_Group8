import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import '../models/throw_data.dart';
import '../services/fake_throw_service.dart';

class _BleThrowUpload {
  final List<ThrowSample> samples = [];
  final StringBuffer rawText = StringBuffer();
  Map<String, dynamic>? metadata;
}

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
  static final Guid _nordicUartServiceUuid = Guid(
    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E",
  );
  static final Guid _nordicUartTxCharacteristicUuid = Guid(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
  );
  static final Guid _nordicUartRxCharacteristicUuid = Guid(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
  );
  static String? _lastFrisbeeTrackRemoteId;

  bool isConnected = false;
  bool isConnecting = false;
  int? queuedThrowCount;
  String bleStatus = "Disconnected";

  BluetoothDevice? connectedDevice;
  BluetoothCharacteristic? uartTxCharacteristic;
  BluetoothCharacteristic? uartRxCharacteristic;

  StreamSubscription<List<ScanResult>>? scanResultsSubscription;
  StreamSubscription<BluetoothConnectionState>? connectionStateSubscription;
  StreamSubscription<List<int>>? uartTxSubscription;

  List<ScanResult> scanResults = [];
  _BleThrowUpload? activeUpload;

  @override
  void initState() {
    super.initState();

    _restorePreviousConnection();
    scanForDevices();
  }

  @override
  void dispose() {
    scanResultsSubscription?.cancel();
    connectionStateSubscription?.cancel();
    uartTxSubscription?.cancel();
    FlutterBluePlus.stopScan();
    super.dispose();
  }

  void safeSetState(VoidCallback update) {
    if (!mounted) {
      return;
    }

    setState(update);
  }

  Future<void> _restorePreviousConnection() async {
    final remoteId = _lastFrisbeeTrackRemoteId;

    if (remoteId == null) {
      return;
    }

    debugPrint("Reconnecting to previous FrisbeeTrack device: $remoteId");

    await connectToDevice(BluetoothDevice.fromId(remoteId), autoConnect: true);
  }

  Future<void> scanForDevices() async {
    await Permission.bluetoothScan.request();
    await Permission.bluetoothConnect.request();
    await Permission.location.request();

    safeSetState(() {
      scanResults.clear();
    });

    await scanResultsSubscription?.cancel();
    scanResultsSubscription = FlutterBluePlus.scanResults.listen((results) {
      safeSetState(() {
        scanResults = results;
      });

      _connectToPreviousDeviceFromScan(results);
    });

    await FlutterBluePlus.stopScan();

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));
  }

  void _connectToPreviousDeviceFromScan(List<ScanResult> results) {
    final remoteId = _lastFrisbeeTrackRemoteId;

    if (remoteId == null || isConnected || isConnecting) {
      return;
    }

    for (final result in results) {
      if (result.device.remoteId.toString() == remoteId) {
        connectToDevice(result.device, autoConnect: true);
        return;
      }
    }
  }

  Future<void> connectToDevice(
    BluetoothDevice device, {
    bool autoConnect = false,
  }) async {
    if (isConnecting) {
      return;
    }

    try {
      safeSetState(() {
        isConnecting = true;
      });

      await _monitorConnectionState(device);

      await device.connect(
        license: License.free,
        autoConnect: autoConnect,
        mtu: autoConnect ? null : 512,
      );
    } catch (e) {
      debugPrint("Connection Error: $e");
    } finally {
      safeSetState(() {
        isConnecting = false;
      });
    }
  }

  Future<void> _monitorConnectionState(BluetoothDevice device) async {
    await connectionStateSubscription?.cancel();

    connectionStateSubscription = device.connectionState.listen((state) async {
      final connected = state == BluetoothConnectionState.connected;

      safeSetState(() {
        connectedDevice = connected ? device : connectedDevice;
        isConnected = connected;
        bleStatus = connected ? "Connected" : "Disconnected";
      });

      if (connected) {
        await _handleDeviceConnected(device);
        return;
      }

      await uartTxSubscription?.cancel();
      uartTxSubscription = null;
      uartTxCharacteristic = null;
      uartRxCharacteristic = null;
      activeUpload = null;
      queuedThrowCount = null;

      if (_lastFrisbeeTrackRemoteId == device.remoteId.toString() && mounted) {
        debugPrint(
          "FrisbeeTrack disconnected; auto reconnect remains enabled.",
        );
      }
    });
  }

  Future<void> _handleDeviceConnected(BluetoothDevice device) async {
    if (!mounted) {
      return;
    }

    connectedDevice = device;

    safeSetState(() {
      isConnected = true;
    });

    final services = await device.discoverServices();

    debugPrint("Services Found: ${services.length}");

    final uartService = _findService(services, _nordicUartServiceUuid);

    if (uartService == null) {
      debugPrint("Nordic UART service not found.");
      return;
    }

    _lastFrisbeeTrackRemoteId = device.remoteId.toString();
    debugPrint("Nordic UART service discovered.");

    final txCharacteristic = _findCharacteristic(
      uartService,
      _nordicUartTxCharacteristicUuid,
    );
    final rxCharacteristic = _findCharacteristic(
      uartService,
      _nordicUartRxCharacteristicUuid,
    );

    if (txCharacteristic == null) {
      debugPrint("Nordic UART TX characteristic not found.");
      return;
    }

    await uartTxSubscription?.cancel();
    uartTxCharacteristic = txCharacteristic;
    uartRxCharacteristic = rxCharacteristic;
    uartTxSubscription = txCharacteristic.onValueReceived.listen((packet) async {
      debugPrint("FrisbeeTrack packet: $packet");

      final textPacket = String.fromCharCodes(packet);
      await _handleBleTextPacket(textPacket);
    });

    await txCharacteristic.setNotifyValue(true);
    await _sendSelectedThrowLabel();
    debugPrint("Subscribed to Nordic UART TX notifications.");
  }

  Future<void> _handleBleTextPacket(String textPacket) async {
    debugPrint("FrisbeeTrack text: ${textPacket.trim()}");

    final upload = activeUpload ?? _BleThrowUpload();
    activeUpload = upload;
    upload.rawText.write(textPacket);

    final lines = upload.rawText.toString().split('\n');
    upload.rawText
      ..clear()
      ..write(lines.removeLast());

    for (final rawLine in lines) {
      final line = rawLine.trim();

      if (line.isEmpty || line.startsWith("sample_index")) {
        continue;
      }

      if (line == "BEGIN_THROW") {
        activeUpload = _BleThrowUpload();
        debugPrint("BLE upload started.");
        continue;
      }

      if (line.startsWith("STATE:")) {
        _handleBleState(line);
        debugPrint("BLE state: $line");
        continue;
      }

      if (line.startsWith("#METADATA,")) {
        activeUpload?.metadata = _parseMetadata(line);
        debugPrint("BLE metadata parsed: ${activeUpload?.metadata}");
        continue;
      }

      if (line == "END_THROW") {
        await _finishBleUpload();
        continue;
      }

      final sample = _parseSampleLine(line);
      if (sample != null) {
        activeUpload?.samples.add(sample);
      } else {
        debugPrint("Skipped unknown BLE line: $line");
      }
    }
  }

  void _handleBleState(String line) {
    final state = line.substring("STATE:".length);

    if (state.startsWith("QUEUE_COUNT:")) {
      final count = int.tryParse(state.substring("QUEUE_COUNT:".length));
      safeSetState(() {
        queuedThrowCount = count;
      });
      return;
    }

    safeSetState(() {
      bleStatus = state;
    });
  }

  Future<void> _sendSelectedThrowLabel([String? selectedLabel]) async {
    final label = selectedLabel ?? widget.selectedThrowType;
    final rxCharacteristic = uartRxCharacteristic;

    if (rxCharacteristic == null) {
      debugPrint("Cannot send label '$label': Nordic UART RX not ready.");
      return;
    }

    try {
      await rxCharacteristic.write(ascii.encode("LABEL:$label"));
      debugPrint("Sent throw label: $label");
    } catch (e) {
      debugPrint("Throw label write failed: $e");
    }
  }

  ThrowSample? _parseSampleLine(String line) {
    final parts = line.split(',');

    if (parts.length != 15) {
      return null;
    }

    try {
      return ThrowSample(
        sampleIndex: int.parse(parts[0]),
        throwId: int.parse(parts[1]),
        label: parts[2],
        timeMs: int.parse(parts[3]),
        ax: double.parse(parts[4]),
        ay: double.parse(parts[5]),
        az: double.parse(parts[6]),
        gx: double.parse(parts[7]),
        gy: double.parse(parts[8]),
        gz: double.parse(parts[9]),
        mx: double.parse(parts[10]),
        my: double.parse(parts[11]),
        mz: double.parse(parts[12]),
        accelMag: double.parse(parts[13]),
        gyroMag: double.parse(parts[14]),
      );
    } catch (e) {
      debugPrint("Sample parse failed: $e | $line");
      return null;
    }
  }

  Map<String, dynamic>? _parseMetadata(String line) {
    try {
      return jsonDecode(line.substring("#METADATA,".length))
          as Map<String, dynamic>;
    } catch (e) {
      debugPrint("Metadata parse failed: $e | $line");
      return null;
    }
  }

  Future<void> _finishBleUpload() async {
    final upload = activeUpload;
    activeUpload = null;

    if (upload == null) {
      debugPrint("END_THROW received without an active upload.");
      return;
    }

    final metadata = upload.metadata;

    if (metadata == null) {
      debugPrint("Throw upload missing metadata; ACK withheld.");
      return;
    }

    final throwId = (metadata["throw_id"] as num).toInt();
    final label = metadata["label"] as String? ?? "unlabeled";
    final flightTimeMs = (metadata["flight_time_ms"] as num?)?.toInt() ?? 0;
    final maxAccel = (metadata["max_accel"] as num?)?.toDouble() ?? 0.0;
    final maxGyro = (metadata["max_gyro"] as num?)?.toDouble() ?? 0.0;
    final expectedSamples = (metadata["samples"] as num?)?.toInt();

    if (expectedSamples != null && expectedSamples != upload.samples.length) {
      debugPrint(
        "Sample count mismatch: expected $expectedSamples, got ${upload.samples.length}. ACK withheld.",
      );
      return;
    }

    widget.onAddThrow(
      ThrowData(
        throwId: throwId,
        label: label,
        flightTime: flightTimeMs / 1000.0,
        maxAccel: maxAccel,
        maxGyro: maxGyro,
        samples: List.unmodifiable(upload.samples),
      ),
    );

    debugPrint(
      "Stored throw $throwId with ${upload.samples.length} samples. Sending ACK.",
    );
    await _acknowledgeThrow();
  }

  Future<void> _acknowledgeThrow() async {
    final rxCharacteristic = uartRxCharacteristic;

    if (rxCharacteristic == null) {
      debugPrint("Cannot ACK throw: Nordic UART RX characteristic not found.");
      return;
    }

    try {
      await rxCharacteristic.write(ascii.encode("ACK_THROW"));
      debugPrint("Sent ACK_THROW.");
    } catch (e) {
      debugPrint("ACK_THROW failed: $e");
    }
  }

  BluetoothService? _findService(List<BluetoothService> services, Guid uuid) {
    for (final service in services) {
      if (service.uuid == uuid) {
        return service;
      }
    }

    return null;
  }

  BluetoothCharacteristic? _findCharacteristic(
    BluetoothService service,
    Guid uuid,
  ) {
    for (final characteristic in service.characteristics) {
      if (characteristic.uuid == uuid) {
        return characteristic;
      }
    }

    return null;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Live Throws")),

      body: Column(
        children: [
          const SizedBox(height: 10),

          //--------------------------------
          // BLE STATUS
          //--------------------------------
          Row(
            mainAxisAlignment: MainAxisAlignment.center,

            children: [
              Icon(
                Icons.circle,
                color: isConnected ? Colors.green : Colors.red,
                size: 14,
              ),

              const SizedBox(width: 8),

              Text(
                isConnected
                    ? "BLE $bleStatus"
                    : isConnecting
                    ? "BLE Connecting"
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

            child: const Text("Scan For Devices"),
          ),

          const SizedBox(height: 8),

          Text(_queueStatusText()),

          const SizedBox(height: 10),

          //--------------------------------
          // DEVICE LIST
          //--------------------------------
          SizedBox(
            height: 150,

            child: ListView.builder(
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
                    onPressed: () {
                      connectToDevice(result.device);
                    },

                    child: const Text("Connect"),
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
              return DropdownMenuItem(value: type, child: Text(type));
            }).toList(),

            onChanged: (value) async {
              widget.onThrowTypeChanged(value);
              await _sendSelectedThrowLabel(value);
            },
          ),

          const SizedBox(height: 10),

          //--------------------------------
          // FAKE THROW
          //--------------------------------
          ElevatedButton(
            onPressed: () {
              final throwData = FakeThrowService.generateThrow(
                widget.liveThrows.length + 1,
              );

              throwData.label = widget.selectedThrowType;

              widget.onAddThrow(throwData);
            },

            child: const Text("Generate Fake Throw"),
          ),

          const SizedBox(height: 10),

          //--------------------------------
          // THROW QUEUE
          //--------------------------------
          Expanded(
            child: ListView.builder(
              itemCount: widget.liveThrows.length,

              itemBuilder: (context, index) {
                final throwData = widget.liveThrows[index];

                return Card(
                  margin: const EdgeInsets.all(8),

                  child: Padding(
                    padding: const EdgeInsets.all(12),

                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,

                      children: [
                        Text(
                          throwData.label,
                          style: const TextStyle(
                            fontSize: 22,
                            fontWeight: FontWeight.bold,
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
                          value: throwData.wobble,

                          title: const Text("Wobbly"),

                          onChanged: (value) {
                            widget.onWobbleChanged(throwData, value ?? false);
                          },
                        ),

                        Row(
                          children: [
                            ElevatedButton(
                              onPressed: () {
                                widget.onSave(throwData);
                              },

                              child: const Text("Save"),
                            ),

                            const SizedBox(width: 8),

                            ElevatedButton(
                              onPressed: () {
                                widget.onDelete(throwData);
                              },

                              child: const Text("Delete"),
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

  String _queueStatusText() {
    final count = queuedThrowCount;

    if (count == null) {
      return "Queue: unknown";
    }

    if (count == 0) {
      return "Queue: empty";
    }

    if (count == 1) {
      return "Queue: 1 throw waiting to upload";
    }

    return "Queue: $count throws waiting to upload";
  }
}
