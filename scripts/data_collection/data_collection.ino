#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>

// =====================================================
// CONFIG
// =====================================================

const float THROW_ACCEL_THRESHOLD = 2.5;   // g
const float THROW_GYRO_THRESHOLD  = 150.0; // deg/sec
const float CATCH_ACCEL_THRESHOLD = 4.0;   // g

const unsigned long MAX_THROW_MS = 4000;
const unsigned long MIN_THROW_MS = 300;

const int PRE_TRIGGER_SAMPLES = 30;
const int MAX_STORAGE_SAMPLES = 800;

// =====================================================
// THROW LABELS
// =====================================================

String currentThrowLabel = "unlabeled";

// =====================================================
// SAMPLE STRUCT
// =====================================================

struct IMUSample {
  uint32_t t; // relative microseconds from throw start

  float ax, ay, az;
  float gx, gy, gz;
  float mx, my, mz;

  float accelMag;
  float gyroMag;
};

// =====================================================
// BUFFERS
// =====================================================

IMUSample preBuffer[PRE_TRIGGER_SAMPLES];
int preIndex = 0;

IMUSample storageBuffer[MAX_STORAGE_SAMPLES];
int storageCount = 0;

// =====================================================
// THROW STATE
// =====================================================

bool recording = false;
bool bleActive = false;

uint32_t throwStartMillis = 0;
uint32_t throwStartMicros = 0;

uint32_t throwID = 0;

// =====================================================
// BLE UUIDS
// Nordic UART Service
// =====================================================

BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");

BLECharacteristic txCharacteristic(
  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
  BLENotify,
  20
);

BLECharacteristic rxCharacteristic(
  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
  BLEWrite | BLEWriteWithoutResponse,
  64
);

// =====================================================
// UTILITY
// =====================================================

float magnitude3(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

// -----------------------------------------------------
// Send BLE text in 20-byte chunks
// -----------------------------------------------------

void blePrint(String msg) {

  int len = msg.length();

  for (int i = 0; i < len; i += 20) {

    int chunkLen = min(20, len - i);

    txCharacteristic.writeValue(
      (const uint8_t*)msg.substring(i, i + chunkLen).c_str(),
      chunkLen
    );

    delay(4); // smaller delay improves throughput
  }
}

// -----------------------------------------------------
// Send MCU state to app
// -----------------------------------------------------

void sendState(String state) {
  blePrint("STATE:" + state + "\n");
}

// -----------------------------------------------------
// Format CSV row
// -----------------------------------------------------

String formatSampleCSV(uint32_t id, IMUSample s) {

  return
    String(id) + "," +
    currentThrowLabel + "," +
    String(s.t) + "," +

    String(s.ax, 4) + "," +
    String(s.ay, 4) + "," +
    String(s.az, 4) + "," +

    String(s.gx, 2) + "," +
    String(s.gy, 2) + "," +
    String(s.gz, 2) + "," +

    String(s.mx, 1) + "," +
    String(s.my, 1) + "," +
    String(s.mz, 1) + "," +

    String(s.accelMag, 4) + "," +
    String(s.gyroMag, 2) + "\n";
}

// =====================================================
// HANDLE APP COMMANDS
// =====================================================

void processBLECommand(String cmd) {

  cmd.trim();

  // -------------------------------
  // THROW LABEL
  // -------------------------------

  if (cmd.startsWith("LABEL:")) {

    currentThrowLabel = cmd.substring(6);

    blePrint("ACK:LABEL:" + currentThrowLabel + "\n");
  }

  // -------------------------------
  // STATUS REQUEST
  // -------------------------------

  else if (cmd == "STATUS") {

    if (recording) {
      sendState("RECORDING");
    }
    else if (storageCount > 0) {
      sendState("UPLOAD_READY");
    }
    else {
      sendState("IDLE");
    }
  }

  // -------------------------------
  // CLEAR THROW
  // -------------------------------

  else if (cmd == "CLEAR") {

    storageCount = 0;

    sendState("IDLE");
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  delay(2000);

  if (!IMU.begin()) {
    while (1);
  }

  if (!BLE.begin()) {
    while (1);
  }

  BLE.setLocalName("FrisbeeTrack");

  BLE.setAdvertisedService(uartService);

  uartService.addCharacteristic(txCharacteristic);
  uartService.addCharacteristic(rxCharacteristic);

  BLE.addService(uartService);

  BLE.advertise();

  bleActive = true;
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // =====================================================
  // BLE CENTRAL
  // =====================================================

  BLEDevice central = BLE.central();

  // =====================================================
  // HANDLE APP COMMANDS
  // =====================================================

  if (rxCharacteristic.written()) {

    String cmd = rxCharacteristic.value();

    processBLECommand(cmd);
  }

  // =====================================================
  // IMU SAMPLE
  // =====================================================

  IMUSample sample;

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(sample.ax, sample.ay, sample.az);
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(sample.gx, sample.gy, sample.gz);
  }

  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(sample.mx, sample.my, sample.mz);
  }

  sample.accelMag = magnitude3(sample.ax, sample.ay, sample.az);

  sample.gyroMag = magnitude3(sample.gx, sample.gy, sample.gz);

  // =====================================================
  // PREBUFFER
  // =====================================================

  if (!recording) {

    sample.t = 0;

    preBuffer[preIndex] = sample;

    preIndex++;

    if (preIndex >= PRE_TRIGGER_SAMPLES) {
      preIndex = 0;
    }
  }

  // =====================================================
  // THROW START DETECTION
  // =====================================================

  bool throwDetected =
    !recording &&
    storageCount == 0 &&
    sample.accelMag > THROW_ACCEL_THRESHOLD &&
    sample.gyroMag > THROW_GYRO_THRESHOLD;

  if (throwDetected) {

    recording = true;

    throwID++;

    storageCount = 0;

    throwStartMillis = millis();
    throwStartMicros = micros();

    sendState("THROW_DETECTED");

    // -----------------------------------------
    // COPY PREBUFFER
    // -----------------------------------------

    int idx = preIndex;

    for (int i = 0; i < PRE_TRIGGER_SAMPLES; i++) {

      if (storageCount < MAX_STORAGE_SAMPLES) {

        preBuffer[idx].t = 0;

        storageBuffer[storageCount] = preBuffer[idx];

        storageCount++;
      }

      idx++;

      if (idx >= PRE_TRIGGER_SAMPLES) {
        idx = 0;
      }
    }

    sendState("RECORDING");
  }

  // =====================================================
  // ACTIVE RECORDING
  // =====================================================

  if (recording) {

    sample.t = micros() - throwStartMicros;

    if (storageCount < MAX_STORAGE_SAMPLES) {

      storageBuffer[storageCount] = sample;

      storageCount++;
    }

    uint32_t throwDuration = millis() - throwStartMillis;

    bool catchDetected =
      throwDuration > 150 &&
      sample.accelMag > CATCH_ACCEL_THRESHOLD;

    bool timeoutDetected =
      throwDuration > MAX_THROW_MS;

    if (catchDetected || timeoutDetected) {

      recording = false;

      sendState("UPLOAD_READY");
    }
  }

  // =====================================================
  // THROW UPLOAD
  // =====================================================

  if (
    !recording &&
    storageCount > 0 &&
    central &&
    central.connected() &&
    txCharacteristic.subscribed()
  ) {

    sendState("UPLOADING");

    // -----------------------------------------
    // CSV HEADER
    // -----------------------------------------

    blePrint(
      "throw_id,label,time_us,"
      "ax,ay,az,"
      "gx,gy,gz,"
      "mx,my,mz,"
      "accel_mag,gyro_mag\n"
    );

    // -----------------------------------------
    // CSV DATA
    // -----------------------------------------

    for (int i = 0; i < storageCount; i++) {

      blePrint(
        formatSampleCSV(
          throwID,
          storageBuffer[i]
        )
      );
    }

    // -----------------------------------------
    // METADATA
    // -----------------------------------------

    blePrint(
      "#METADATA,{\"throw_id\":" +
      String(throwID) +
      ",\"label\":\"" +
      currentThrowLabel +
      "\",\"samples\":" +
      String(storageCount) +
      "}\n"
    );

    sendState("UPLOAD_COMPLETE");

    storageCount = 0;
  }

  delay(5);
}
