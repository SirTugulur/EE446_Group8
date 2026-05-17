#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>

// =====================================================
// CONFIG
// =====================================================

const float THROW_ACCEL_THRESHOLD = 2.5;   // g
const float THROW_GYRO_THRESHOLD  = 150.0; // deg/sec
const float CATCH_ACCEL_THRESHOLD = 4.0;   // impact spike

const unsigned long MAX_THROW_MS = 4000;
const unsigned long MIN_THROW_MS = 300;

const int PRE_TRIGGER_SAMPLES = 30;
const int MAX_STORAGE_SAMPLES = 800;

// =====================================================
// THROW LABEL
// =====================================================

String currentThrowLabel = "unlabeled";

// =====================================================
// SAMPLE STRUCT
// =====================================================

struct IMUSample {
  unsigned long t;

  float ax, ay, az;
  float gx, gy, gz;
  float mx, my, mz;

  float accelMag;
  float gyroMag;
};

// =====================================================
// MEMORY BUFFERS
// =====================================================

IMUSample preBuffer[PRE_TRIGGER_SAMPLES];
int preIndex = 0;

IMUSample storageBuffer[MAX_STORAGE_SAMPLES];
int storageCount = 0;

// =====================================================
// THROW STATE
// =====================================================

bool recording = false;

unsigned long throwStartTime = 0;
unsigned long throwID = 0;

bool bleActive = false;

// =====================================================
// BLE UART UUIDS
// =====================================================

BLEService uartService(
  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
);

BLECharacteristic txCharacteristic(
  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
  BLENotify,
  20
);

BLECharacteristic rxCharacteristic(
  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
  BLEWrite,
  20
);

// =====================================================
// UTILITIES
// =====================================================

float magnitude3(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

// =====================================================
// BLE PRINT
// =====================================================

void blePrint(String msg) {

  int len = msg.length();

  for (int i = 0; i < len; i += 20) {

    int chunkLen = min(20, len - i);

    String chunk = msg.substring(i, i + chunkLen);

    txCharacteristic.writeValue(
      (const uint8_t*)chunk.c_str(),
      chunkLen
    );

    delay(10);
  }
}

// =====================================================
// CSV FORMATTER
// =====================================================

String formatSampleCSV(
  unsigned long id,
  String label,
  int sampleIndex,
  unsigned long tRel,
  IMUSample s
) {

  return String(sampleIndex) + "," +
         String(id) + "," +
         label + "," +
         String(tRel) + "," +

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
// BLE COMMAND PROCESSING
// =====================================================

void processBLECommand(String cmd) {

  cmd.trim();

  if (cmd.startsWith("LABEL:")) {

    currentThrowLabel = cmd.substring(6);

    blePrint(
      "STATE:LABEL_SET:" +
      currentThrowLabel +
      "\n"
    );
  }

  else if (cmd == "STATUS") {

    blePrint("STATE:READY\n");
  }

  else if (cmd == "CLEAR") {

    storageCount = 0;

    blePrint("STATE:CLEARED\n");
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

  BLEDevice central = BLE.central();

  // =====================================================
  // HANDLE BLE COMMANDS
  // =====================================================

  if (central && central.connected()) {

    if (rxCharacteristic.written()) {

      const uint8_t* rawData =
        rxCharacteristic.value();

      String cmd =
        String((char*)rawData);

      processBLECommand(cmd);
    }
  }

  // =====================================================
  // READ IMU
  // =====================================================

  IMUSample sample;

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(
      sample.ax,
      sample.ay,
      sample.az
    );
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(
      sample.gx,
      sample.gy,
      sample.gz
    );
  }

  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(
      sample.mx,
      sample.my,
      sample.mz
    );
  }

  sample.t = micros();

  sample.accelMag = magnitude3(
    sample.ax,
    sample.ay,
    sample.az
  );

  sample.gyroMag = magnitude3(
    sample.gx,
    sample.gy,
    sample.gz
  );

  // =====================================================
  // PRE-TRIGGER BUFFER
  // =====================================================

  if (!recording) {

    preBuffer[preIndex] = sample;

    preIndex++;

    if (preIndex >= PRE_TRIGGER_SAMPLES) {
      preIndex = 0;
    }
  }

  // =====================================================
  // THROW START DETECTION
  // =====================================================

  if (!recording &&
      storageCount == 0 &&
      sample.accelMag > THROW_ACCEL_THRESHOLD &&
      sample.gyroMag > THROW_GYRO_THRESHOLD) {

    recording = true;

    throwStartTime = millis();

    throwID++;

    storageCount = 0;

    int idx = preIndex;

    for (int i = 0; i < PRE_TRIGGER_SAMPLES; i++) {

      if (storageCount < MAX_STORAGE_SAMPLES) {

        storageBuffer[storageCount] =
          preBuffer[idx];

        storageCount++;
      }

      idx++;

      if (idx >= PRE_TRIGGER_SAMPLES) {
        idx = 0;
      }
    }

    blePrint("STATE:RECORDING\n");
  }

  // =====================================================
  // ACTIVE RECORDING
  // =====================================================

  if (recording) {

    if (storageCount < MAX_STORAGE_SAMPLES) {

      storageBuffer[storageCount] = sample;

      storageCount++;
    }

    unsigned long throwDuration =
      millis() - throwStartTime;

    bool catchDetected =
      (throwDuration > 150 &&
       sample.accelMag > CATCH_ACCEL_THRESHOLD);

    bool timeoutDetected =
      (throwDuration > MAX_THROW_MS);

    if (catchDetected || timeoutDetected) {

      recording = false;

      blePrint("STATE:UPLOAD_READY\n");
    }
  }

  // =====================================================
  // DATA UPLOAD
  // =====================================================

  if (!recording &&
      storageCount > 0 &&
      central &&
      central.connected() &&
      txCharacteristic.subscribed()) {

    // BEGIN THROW
    blePrint("BEGIN_THROW\n");

    // Upload state
    blePrint("STATE:UPLOADING\n");

    // CSV Header
    blePrint(
      "sample_index,throw_id,label,time_ms,"
      "ax,ay,az,gx,gy,gz,mx,my,mz,"
      "accel_mag,gyro_mag\n"
    );

    unsigned long t0 = storageBuffer[0].t;

    // Send samples
    for (int i = 0; i < storageCount; i++) {

      unsigned long relativeMs =
        (storageBuffer[i].t - t0) / 1000;

      blePrint(
        formatSampleCSV(
          throwID,
          currentThrowLabel,
          i,
          relativeMs,
          storageBuffer[i]
        )
      );
    }

    // Metadata
    blePrint(
      "#METADATA,"
      "{\"throw_id\":" +
      String(throwID) +
      ",\"label\":\"" +
      currentThrowLabel +
      "\",\"samples\":" +
      String(storageCount) +
      "}\n"
    );

    // End marker
    blePrint("END_THROW\n");

    // Upload complete
    blePrint("STATE:UPLOAD_COMPLETE\n");

    // Reset storage
    storageCount = 0;

    // Reset label
    currentThrowLabel = "unlabeled";
  }

  delay(5);
}
