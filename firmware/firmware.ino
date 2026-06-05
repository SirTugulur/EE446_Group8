#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>

// =====================================================
// CONFIG
// =====================================================

const float THROW_ACCEL_THRESHOLD = 2.5;   // g
const float THROW_GYRO_THRESHOLD  = 150.0; // deg/sec

// NEW: Gyro decay thresholds for catch/drop detection
const float CATCH_GYRO_THRESHOLD  = 100.0;  // deg/sec (spin has mostly stopped)
const unsigned long CATCH_DEBOUNCE_MS = 200; // How long spin must remain dead

const unsigned long MAX_THROW_MS = 5000;
const unsigned long MIN_THROW_MS = 250;

const int PRE_TRIGGER_SAMPLES = 30;
const int MAX_STORAGE_SAMPLES = 550; // REDUCED: Protects SRAM
const int MAX_THROWS = 2;            // REDUCED: Protects SRAM
const int MAX_LABEL_LEN = 24;
const unsigned long ACK_TIMEOUT_MS = 10000;
const int UPLOAD_PACKET_DELAY_MS = 2;
const int MAX_UPLOAD_PACKET_BYTES = 220;

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

struct CompletedThrow {
  unsigned long id;
  char label[MAX_LABEL_LEN];
  unsigned long flightTimeMs;
  float maxAccel;
  float maxGyro;
  int sampleCount;
  bool waitingForAck;
  unsigned long uploadCompleteMs;
  IMUSample samples[MAX_STORAGE_SAMPLES];
};

// =====================================================
// MEMORY BUFFERS
// =====================================================

IMUSample preBuffer[PRE_TRIGGER_SAMPLES];
int preIndex = 0;

IMUSample recordingBuffer[MAX_STORAGE_SAMPLES];
int recordingCount = 0;

CompletedThrow throwQueue[MAX_THROWS];
int queueCount = 0;

// =====================================================
// THROW STATE
// =====================================================

bool recording = false;
bool connected = false;
bool subscribed = false;

unsigned long throwStartTime = 0;
unsigned long throwID = 0;
float activeMaxAccel = 0.0;
float activeMaxGyro = 0.0;

// NEW: Gyro Catch State
unsigned long lowSpinStartTime = 0;
bool inLowSpin = false;

// NEW: Async Upload State Machine
enum UploadPhase { IDLE, HEADER, SAMPLES, METADATA };
UploadPhase uploadPhase = IDLE;
int uploadSampleIndex = 0;

// ---------- BLE NORDIC UART UUIDS ----------
BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic txCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 255);
BLECharacteristic rxCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, 255);

// =====================================================
// UTILITIES
// =====================================================

float magnitude3(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

void copyLabel(char* destination, const String& source) {
  source.substring(0, MAX_LABEL_LEN - 1).toCharArray(destination, MAX_LABEL_LEN);
}

// =====================================================
// BLE PRINT
// =====================================================

bool blePrint(String msg) {
  if (!connected || !txCharacteristic.subscribed()) {
    return false;
  }

  txCharacteristic.writeValue((const uint8_t*)msg.c_str(), msg.length());
  delay(UPLOAD_PACKET_DELAY_MS);
  return true;
}

void sendState(const String& state) {
  blePrint("STATE:" + state + "\n");
  Serial.println("STATE:" + state);
}

void sendQueueCount() {
  sendState("QUEUE_COUNT:" + String(queueCount));
}

// =====================================================
// CSV FORMATTER
// =====================================================

String formatSampleCSV(unsigned long id, const char* label, int sampleIndex, unsigned long tRel, IMUSample s) {
  return String(sampleIndex) + "," + String(id) + "," + String(label) + "," + String(tRel) + "," +
         String(s.ax, 4) + "," + String(s.ay, 4) + "," + String(s.az, 4) + "," +
         String(s.gx, 2) + "," + String(s.gy, 2) + "," + String(s.gz, 2) + "," +
         String(s.mx, 1) + "," + String(s.my, 1) + "," + String(s.mz, 1) + "," +
         String(s.accelMag, 4) + "," + String(s.gyroMag, 2) + "\n";
}

// =====================================================
// QUEUE
// =====================================================

void clearQueue() {
  queueCount = 0;
  uploadPhase = IDLE;

  for (int i = 0; i < MAX_THROWS; i++) {
    throwQueue[i].waitingForAck = false;
  }
}

void dequeueOldestThrow() {
  if (queueCount <= 0) {
    return;
  }

  for (int i = 1; i < queueCount; i++) {
    throwQueue[i - 1] = throwQueue[i];
  }

  queueCount--;
}

void queueCompletedThrow(unsigned long flightTimeMs) {
  if (queueCount >= MAX_THROWS) {
    if (connected) {
      sendState("QUEUE_FULL");
      sendQueueCount();
    }
    return;
  }

  CompletedThrow& queuedThrow = throwQueue[queueCount];
  queuedThrow.id = throwID;
  copyLabel(queuedThrow.label, currentThrowLabel);
  queuedThrow.flightTimeMs = flightTimeMs;
  queuedThrow.maxAccel = activeMaxAccel;
  queuedThrow.maxGyro = activeMaxGyro;
  queuedThrow.sampleCount = recordingCount;
  queuedThrow.waitingForAck = false;
  queuedThrow.uploadCompleteMs = 0;

  for (int i = 0; i < recordingCount; i++) {
    queuedThrow.samples[i] = recordingBuffer[i];
  }

  queueCount++;
  sendState("UPLOAD_READY");
  sendQueueCount();
}

// =====================================================
// BLE COMMAND PROCESSING
// =====================================================

void processBLECommand(String cmd) {
  cmd.trim();

  if (cmd.startsWith("LABEL:")) {
    currentThrowLabel = cmd.substring(6);
    blePrint("STATE:LABEL_SET:" + currentThrowLabel + "\n");
  }
  else if (cmd == "ACK_THROW") {
    if (queueCount > 0 && throwQueue[0].waitingForAck) {
      dequeueOldestThrow();
      sendState("THROW_CONFIRMED");

      if (queueCount == 0) {
        sendState("QUEUE_EMPTY");
      } else {
        sendState("UPLOAD_READY");
      }

      sendQueueCount();
    } else {
      Serial.println("ACK_THROW ignored: no uploaded throw waiting for ACK");
    }
  }
  else if (cmd == "STATUS") {
    if (queueCount == 0) {
      sendState("QUEUE_EMPTY");
    } else {
      sendState("UPLOAD_READY");
    }
    sendQueueCount();
  }
  else if (cmd == "CLEAR") {
    clearQueue();
    recordingCount = 0;
    blePrint("STATE:CLEARED\n");
    sendQueueCount();
  }
}

String readBLECommand() {
  String cmd = "";
  int len = rxCharacteristic.valueLength();
  const uint8_t* rawData = rxCharacteristic.value();

  for (int i = 0; i < len; i++) {
    cmd += (char)rawData[i];
  }
  return cmd;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  delay(2000);
  Serial.begin(115200);

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
}

// =====================================================
// ASYNC UPLOAD STATE MACHINE
// =====================================================

void handleAsyncUpload() {
  // Don't upload if queue is empty, waiting for ack, or disconnected
  if (queueCount == 0 || throwQueue[0].waitingForAck || !connected || !txCharacteristic.subscribed()) {
    return;
  }

  CompletedThrow& queuedThrow = throwQueue[0];

  switch(uploadPhase) {
    case IDLE:
      sendState("UPLOADING");
      if (blePrint("BEGIN_THROW\n")) {
        uploadPhase = HEADER;
      }
      break;

    case HEADER:
      if (blePrint("sample_index,throw_id,label,time_ms,ax,ay,az,gx,gy,gz,mx,my,mz,accel_mag,gyro_mag\n")) {
        uploadSampleIndex = 0;
        uploadPhase = SAMPLES;
      }
      break;

    case SAMPLES:
      if (uploadSampleIndex < queuedThrow.sampleCount) {
        unsigned long t0 = queuedThrow.samples[0].t;
        String packet = "";

        while (uploadSampleIndex < queuedThrow.sampleCount) {
          unsigned long relativeMs = (queuedThrow.samples[uploadSampleIndex].t - t0) / 1000;
          String row = formatSampleCSV(
            queuedThrow.id,
            queuedThrow.label,
            uploadSampleIndex,
            relativeMs,
            queuedThrow.samples[uploadSampleIndex]
          );

          if (packet.length() > 0 &&
              packet.length() + row.length() > MAX_UPLOAD_PACKET_BYTES) {
            break;
          }

          packet += row;
          uploadSampleIndex++;
        }

        if (packet.length() > 0 && !blePrint(packet)) {
          return;
        }
      } else {
        uploadPhase = METADATA;
      }
      break;

    case METADATA:
      String meta = "#METADATA,{\"throw_id\":" + String(queuedThrow.id) + 
                    ", \"label\":\"" + String(queuedThrow.label) + 
                    "\", \"samples\":" + String(queuedThrow.sampleCount) + 
                    ", \"flight_time_ms\":" + String(queuedThrow.flightTimeMs) + 
                    ", \"max_accel\":" + String(queuedThrow.maxAccel, 4) + 
                    ", \"max_gyro\":" + String(queuedThrow.maxGyro, 2) + "}\n";
                    
      if (blePrint(meta) && blePrint("END_THROW\n")) {
        queuedThrow.waitingForAck = true;
        queuedThrow.uploadCompleteMs = millis();
        sendState("UPLOAD_COMPLETE");
        sendQueueCount();
        uploadPhase = IDLE; // Reset phase for the next throw
      }
      break;
  }
}

void checkAckTimeout() {
  if (queueCount == 0 || !throwQueue[0].waitingForAck) {
    return;
  }

  if (!connected || !txCharacteristic.subscribed()) {
    return;
  }

  unsigned long elapsed = millis() - throwQueue[0].uploadCompleteMs;

  if (elapsed >= ACK_TIMEOUT_MS) {
    throwQueue[0].waitingForAck = false;
    throwQueue[0].uploadCompleteMs = 0;
    Serial.println("ACK timeout; oldest throw will retry upload");
    sendState("UPLOAD_READY");
  }
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  BLEDevice central = BLE.central();
  bool nowConnected = central && central.connected();

  if (nowConnected && !connected) {
    connected = true;
    Serial.println("STATE:CONNECTED");
  } else if (!nowConnected && connected) {
    sendState("DISCONNECTED");
    connected = false;
    subscribed = false;
    uploadPhase = IDLE;
    Serial.println("STATE:DISCONNECTED");
  }

  bool nowSubscribed = connected && txCharacteristic.subscribed();

  if (nowSubscribed && !subscribed) {
    subscribed = true;
    sendState("CONNECTED");

    if (queueCount == 0) {
      sendState("QUEUE_EMPTY");
    } else {
      sendState("UPLOAD_READY");
    }
    sendQueueCount();
  } else if (!nowSubscribed && subscribed) {
    subscribed = false;
    uploadPhase = IDLE;
  }

  // =====================================================
  // HANDLE BLE COMMANDS
  // =====================================================

  if (connected && rxCharacteristic.written()) {
    processBLECommand(readBLECommand());
  }

  // =====================================================
  // READ IMU
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

  sample.t = micros();
  sample.accelMag = magnitude3(sample.ax, sample.ay, sample.az);
  sample.gyroMag = magnitude3(sample.gx, sample.gy, sample.gz);

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
      sample.accelMag > THROW_ACCEL_THRESHOLD &&
      sample.gyroMag > THROW_GYRO_THRESHOLD) {

    recording = true;
    throwStartTime = millis();
    throwID++;
    recordingCount = 0;
    activeMaxAccel = 0.0;
    activeMaxGyro = 0.0;
    inLowSpin = false;

    int idx = preIndex;

    for (int i = 0; i < PRE_TRIGGER_SAMPLES; i++) {
      if (recordingCount < MAX_STORAGE_SAMPLES) {
        recordingBuffer[recordingCount] = preBuffer[idx];
        activeMaxAccel = max(activeMaxAccel, preBuffer[idx].accelMag);
        activeMaxGyro = max(activeMaxGyro, preBuffer[idx].gyroMag);
        recordingCount++;
      }

      idx++;
      if (idx >= PRE_TRIGGER_SAMPLES) {
        idx = 0;
      }
    }
    sendState("RECORDING");
  }

  // =====================================================
  // ACTIVE RECORDING & CATCH DETECTION
  // =====================================================

  if (recording) {

    if (recordingCount < MAX_STORAGE_SAMPLES) {
      recordingBuffer[recordingCount] = sample;
      activeMaxAccel = max(activeMaxAccel, sample.accelMag);
      activeMaxGyro = max(activeMaxGyro, sample.gyroMag);
      recordingCount++;
    }

    unsigned long throwDuration = millis() - throwStartTime;

    // Wait until MIN_THROW_MS before checking for a catch
    if (throwDuration > MIN_THROW_MS) {
      
      // GYRO DECAY DETECTION
      if (sample.gyroMag < CATCH_GYRO_THRESHOLD) {
        if (!inLowSpin) {
          inLowSpin = true;
          lowSpinStartTime = millis();
        } else if ((millis() - lowSpinStartTime) > CATCH_DEBOUNCE_MS) {
          recording = false;
          Serial.println("Flight finished (Spin decay)");
          queueCompletedThrow(throwDuration);
          recordingCount = 0;
          inLowSpin = false; 
        }
      } else {
        inLowSpin = false; // Reset if it spins back up
      }
    }

    // TIMEOUT FALLBACK
    if (recording && throwDuration > MAX_THROW_MS) {
      recording = false;
      Serial.println("Flight finished (Timeout)");
      queueCompletedThrow(throwDuration);
      recordingCount = 0;
      inLowSpin = false;
    }
  }

  // =====================================================
  // DATA UPLOAD (Non-Blocking)
  // =====================================================

  if (!recording && queueCount > 0) {
    handleAsyncUpload();
  }

  checkAckTimeout();
}
