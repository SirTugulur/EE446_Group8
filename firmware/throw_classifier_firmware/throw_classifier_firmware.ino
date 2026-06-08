#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>

// Paste or add your Edge Impulse Arduino library header next to this sketch.
// Example exported header name:
//   #include "frisbee_throw_classifier_inferencing.h"
#if __has_include("frisbee_throw_classifier_inferencing.h")
#include "frisbee_throw_classifier_inferencing.h"
#define HAS_EDGE_IMPULSE_MODEL 1
#else
#define HAS_EDGE_IMPULSE_MODEL 0
#endif

// =====================================================
// CONFIG
// =====================================================

const float THROW_ACCEL_THRESHOLD = 2.5f;
const float THROW_GYRO_THRESHOLD = 150.0f;
const float STOP_GYRO_THRESHOLD = 100.0f;
const unsigned long STOP_DEBOUNCE_MS = 200;

const unsigned long MIN_THROW_MS = 250;
const unsigned long MAX_THROW_MS = 5000;

const int PRE_TRIGGER_SAMPLES = 30;
const int MAX_STORAGE_SAMPLES = 550;
const int MAX_THROWS = 2;
const int MAX_LABEL_LEN = 24;
const int MAX_UPLOAD_PACKET_BYTES = 220;
const int UPLOAD_PACKET_DELAY_MS = 2;
const unsigned long ACK_TIMEOUT_MS = 10000;

// Keep this in sync with the features used by the Edge Impulse impulse.
// This sketch fills ax, ay, az, gx, gy, gz.
const int MODEL_FEATURE_AXES = 6;

#if HAS_EDGE_IMPULSE_MODEL
const int MODEL_FEATURE_COUNT = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
const int MODEL_WINDOW_SAMPLES = MODEL_FEATURE_COUNT / MODEL_FEATURE_AXES;
float eiFeatures[MODEL_FEATURE_COUNT];
#else
const int MODEL_WINDOW_SAMPLES = 128;
#endif

// =====================================================
// BLE NORDIC UART UUIDS
// =====================================================

BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic txCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 255);
BLECharacteristic rxCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, 255);

// =====================================================
// DATA TYPES
// =====================================================

enum DeviceMode {
  DATA_COLLECTION,
  THROW_CLASSIFICATION,
};

enum UploadPhase {
  IDLE,
  HEADER,
  SAMPLES,
  METADATA,
};

struct IMUSample {
  unsigned long t;
  float ax, ay, az;
  float gx, gy, gz;
  float mx, my, mz;
  float accelMag;
  float gyroMag;
};

struct ThrowPrediction {
  char label[MAX_LABEL_LEN];
  float confidence;
  bool wobbly;
  bool completed;
};

struct CompletedThrow {
  unsigned long id;
  char label[MAX_LABEL_LEN];
  unsigned long flightTimeMs;
  float maxAccel;
  float maxGyro;
  int sampleCount;
  bool wobbly;
  bool completed;
  float confidence;
  bool waitingForAck;
  unsigned long uploadCompleteMs;
  IMUSample samples[MAX_STORAGE_SAMPLES];
};

// =====================================================
// STATE
// =====================================================

DeviceMode deviceMode = THROW_CLASSIFICATION;
String collectionLabel = "unlabeled";

IMUSample preBuffer[PRE_TRIGGER_SAMPLES];
int preIndex = 0;

IMUSample recordingBuffer[MAX_STORAGE_SAMPLES];
int recordingCount = 0;

CompletedThrow throwQueue[MAX_THROWS];
int queueCount = 0;

bool recording = false;
bool connected = false;
bool subscribed = false;
bool inLowSpin = false;

unsigned long throwStartTime = 0;
unsigned long lowSpinStartTime = 0;
unsigned long throwId = 0;

float activeMaxAccel = 0.0f;
float activeMaxGyro = 0.0f;

UploadPhase uploadPhase = IDLE;
int uploadSampleIndex = 0;

// =====================================================
// UTILITIES
// =====================================================

float magnitude3(float x, float y, float z) {
  return sqrt((x * x) + (y * y) + (z * z));
}

void copyLabel(char* destination, const String& source) {
  source.substring(0, MAX_LABEL_LEN - 1).toCharArray(destination, MAX_LABEL_LEN);
}

void copyLabel(char* destination, const char* source) {
  strncpy(destination, source, MAX_LABEL_LEN - 1);
  destination[MAX_LABEL_LEN - 1] = '\0';
}

bool blePrint(const String& msg) {
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

String boolText(bool value) {
  return value ? "true" : "false";
}

// =====================================================
// EDGE IMPULSE INFERENCE
// =====================================================

#if HAS_EDGE_IMPULSE_MODEL
int getEiFeatureData(size_t offset, size_t length, float* outPtr) {
  memcpy(outPtr, eiFeatures + offset, length * sizeof(float));
  return 0;
}
#endif

void buildModelFeatures() {
#if HAS_EDGE_IMPULSE_MODEL
  int start = recordingCount - MODEL_WINDOW_SAMPLES;
  if (start < 0) {
    start = 0;
  }

  for (int i = 0; i < MODEL_WINDOW_SAMPLES; i++) {
    int sampleIndex = start + i;
    if (sampleIndex >= recordingCount) {
      sampleIndex = recordingCount - 1;
    }
    if (sampleIndex < 0) {
      sampleIndex = 0;
    }

    const IMUSample& sample = recordingBuffer[sampleIndex];
    int featureIndex = i * MODEL_FEATURE_AXES;
    eiFeatures[featureIndex + 0] = sample.ax;
    eiFeatures[featureIndex + 1] = sample.ay;
    eiFeatures[featureIndex + 2] = sample.az;
    eiFeatures[featureIndex + 3] = sample.gx;
    eiFeatures[featureIndex + 4] = sample.gy;
    eiFeatures[featureIndex + 5] = sample.gz;
  }
#endif
}

ThrowPrediction classifyRecordedThrow(unsigned long flightTimeMs) {
  ThrowPrediction prediction;
  copyLabel(prediction.label, collectionLabel);
  prediction.confidence = 0.0f;
  prediction.wobbly = false;
  prediction.completed = true;

#if HAS_EDGE_IMPULSE_MODEL
  buildModelFeatures();

  signal_t signal;
  signal.total_length = MODEL_FEATURE_COUNT;
  signal.get_data = &getEiFeatureData;

  ei_impulse_result_t result = {0};
  EI_IMPULSE_ERROR error = run_classifier(&signal, &result, false);

  if (error == EI_IMPULSE_OK) {
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
      const char* label = result.classification[i].label;
      float value = result.classification[i].value;

      if (strstr(label, "wobble") || strstr(label, "wobbly")) {
        prediction.wobbly = value >= 0.5f;
        continue;
      }

      if (strstr(label, "complete") || strstr(label, "caught")) {
        prediction.completed = value >= 0.5f;
        continue;
      }

      if (value > prediction.confidence) {
        prediction.confidence = value;
        copyLabel(prediction.label, label);
      }
    }
  } else {
    copyLabel(prediction.label, "model_error");
  }
#else
  // Fallback keeps the sketch useful before the model is pasted in.
  prediction.wobbly = activeMaxGyro > 900.0f || activeMaxAccel > 8.0f;
  prediction.completed = flightTimeMs > 350 && activeMaxAccel < 18.0f;
  prediction.confidence = 0.0f;
#endif

  if (deviceMode == DATA_COLLECTION) {
    copyLabel(prediction.label, collectionLabel);
  }

  return prediction;
}

// =====================================================
// QUEUE / UPLOAD
// =====================================================

String formatSampleCSV(unsigned long id, const char* label, int sampleIndex, unsigned long tRel, IMUSample s) {
  return String(sampleIndex) + "," + String(id) + "," + String(label) + "," + String(tRel) + "," +
         String(s.ax, 4) + "," + String(s.ay, 4) + "," + String(s.az, 4) + "," +
         String(s.gx, 2) + "," + String(s.gy, 2) + "," + String(s.gz, 2) + "," +
         String(s.mx, 1) + "," + String(s.my, 1) + "," + String(s.mz, 1) + "," +
         String(s.accelMag, 4) + "," + String(s.gyroMag, 2) + "\n";
}

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
    sendState("QUEUE_FULL");
    sendQueueCount();
    return;
  }

  ThrowPrediction prediction = classifyRecordedThrow(flightTimeMs);
  CompletedThrow& queuedThrow = throwQueue[queueCount];

  queuedThrow.id = throwId;
  copyLabel(queuedThrow.label, prediction.label);
  queuedThrow.flightTimeMs = flightTimeMs;
  queuedThrow.maxAccel = activeMaxAccel;
  queuedThrow.maxGyro = activeMaxGyro;
  queuedThrow.sampleCount = recordingCount;
  queuedThrow.wobbly = prediction.wobbly;
  queuedThrow.completed = prediction.completed;
  queuedThrow.confidence = prediction.confidence;
  queuedThrow.waitingForAck = false;
  queuedThrow.uploadCompleteMs = 0;

  for (int i = 0; i < recordingCount; i++) {
    queuedThrow.samples[i] = recordingBuffer[i];
  }

  queueCount++;
  sendState(deviceMode == DATA_COLLECTION ? "COLLECTION_THROW_READY" : "CLASSIFIED_THROW_READY");
  sendQueueCount();
}

void handleAsyncUpload() {
  if (queueCount == 0 || throwQueue[0].waitingForAck || !connected || !txCharacteristic.subscribed()) {
    return;
  }

  CompletedThrow& queuedThrow = throwQueue[0];

  switch (uploadPhase) {
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

          if (packet.length() > 0 && packet.length() + row.length() > MAX_UPLOAD_PACKET_BYTES) {
            break;
          }

          packet += row;
          uploadSampleIndex++;
        }

        if (packet.length() > 0) {
          blePrint(packet);
        }
      } else {
        uploadPhase = METADATA;
      }
      break;

    case METADATA: {
      String meta = "#METADATA,{\"throw_id\":" + String(queuedThrow.id) +
                    ",\"mode\":\"" + String(deviceMode == DATA_COLLECTION ? "collection" : "classification") +
                    "\",\"label\":\"" + String(queuedThrow.label) +
                    "\",\"samples\":" + String(queuedThrow.sampleCount) +
                    ",\"flight_time_ms\":" + String(queuedThrow.flightTimeMs) +
                    ",\"max_accel\":" + String(queuedThrow.maxAccel, 4) +
                    ",\"max_gyro\":" + String(queuedThrow.maxGyro, 2) +
                    ",\"wobble\":" + boolText(queuedThrow.wobbly) +
                    ",\"completed\":" + boolText(queuedThrow.completed) +
                    ",\"confidence\":" + String(queuedThrow.confidence, 4) + "}\n";

      if (blePrint(meta) && blePrint("END_THROW\n")) {
        queuedThrow.waitingForAck = true;
        queuedThrow.uploadCompleteMs = millis();
        sendState("UPLOAD_COMPLETE");
        sendQueueCount();
        uploadPhase = IDLE;
      }
      break;
    }
  }
}

void checkAckTimeout() {
  if (queueCount == 0 || !throwQueue[0].waitingForAck) {
    return;
  }

  if (!connected || !txCharacteristic.subscribed()) {
    return;
  }

  if (millis() - throwQueue[0].uploadCompleteMs >= ACK_TIMEOUT_MS) {
    throwQueue[0].waitingForAck = false;
    throwQueue[0].uploadCompleteMs = 0;
    sendState("UPLOAD_READY");
  }
}

// =====================================================
// BLE COMMANDS
// =====================================================

String readBLECommand() {
  String cmd = "";
  int len = rxCharacteristic.valueLength();
  const uint8_t* rawData = rxCharacteristic.value();

  for (int i = 0; i < len; i++) {
    cmd += (char)rawData[i];
  }

  return cmd;
}

void processBLECommand(String cmd) {
  cmd.trim();

  if (cmd.startsWith("LABEL:")) {
    collectionLabel = cmd.substring(6);
    sendState("LABEL_SET:" + collectionLabel);
  } else if (cmd == "MODE:COLLECT") {
    deviceMode = DATA_COLLECTION;
    sendState("MODE:COLLECT");
  } else if (cmd == "MODE:CLASSIFY") {
    deviceMode = THROW_CLASSIFICATION;
    sendState("MODE:CLASSIFY");
  } else if (cmd == "ACK_THROW") {
    if (queueCount > 0 && throwQueue[0].waitingForAck) {
      dequeueOldestThrow();
      sendState("THROW_CONFIRMED");
      sendQueueCount();
    }
  } else if (cmd == "CLEAR") {
    clearQueue();
    recordingCount = 0;
    sendState("CLEARED");
    sendQueueCount();
  } else if (cmd == "STATUS") {
    sendState(deviceMode == DATA_COLLECTION ? "MODE:COLLECT" : "MODE:CLASSIFY");
    sendQueueCount();
  }
}

// =====================================================
// SETUP / LOOP
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

void updateBleConnection() {
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
  }

  bool nowSubscribed = connected && txCharacteristic.subscribed();

  if (nowSubscribed && !subscribed) {
    subscribed = true;
    sendState("CONNECTED");
    sendState(deviceMode == DATA_COLLECTION ? "MODE:COLLECT" : "MODE:CLASSIFY");
    sendQueueCount();
  } else if (!nowSubscribed && subscribed) {
    subscribed = false;
    uploadPhase = IDLE;
  }

  if (connected && rxCharacteristic.written()) {
    processBLECommand(readBLECommand());
  }
}

bool readImuSample(IMUSample& sample) {
  if (!IMU.accelerationAvailable() || !IMU.gyroscopeAvailable()) {
    return false;
  }

  IMU.readAcceleration(sample.ax, sample.ay, sample.az);
  IMU.readGyroscope(sample.gx, sample.gy, sample.gz);

  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(sample.mx, sample.my, sample.mz);
  } else {
    sample.mx = 0.0f;
    sample.my = 0.0f;
    sample.mz = 0.0f;
  }

  sample.t = micros();
  sample.accelMag = magnitude3(sample.ax, sample.ay, sample.az);
  sample.gyroMag = magnitude3(sample.gx, sample.gy, sample.gz);
  return true;
}

void startThrowRecording() {
  recording = true;
  throwStartTime = millis();
  throwId++;
  recordingCount = 0;
  activeMaxAccel = 0.0f;
  activeMaxGyro = 0.0f;
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

void finishThrowRecording(unsigned long throwDuration) {
  recording = false;
  queueCompletedThrow(throwDuration);
  recordingCount = 0;
  inLowSpin = false;
}

void loop() {
  updateBleConnection();

  IMUSample sample;
  if (!readImuSample(sample)) {
    handleAsyncUpload();
    checkAckTimeout();
    return;
  }

  if (!recording) {
    preBuffer[preIndex] = sample;
    preIndex = (preIndex + 1) % PRE_TRIGGER_SAMPLES;
  }

  if (!recording &&
      sample.accelMag > THROW_ACCEL_THRESHOLD &&
      sample.gyroMag > THROW_GYRO_THRESHOLD) {
    startThrowRecording();
  }

  if (recording) {
    if (recordingCount < MAX_STORAGE_SAMPLES) {
      recordingBuffer[recordingCount] = sample;
      activeMaxAccel = max(activeMaxAccel, sample.accelMag);
      activeMaxGyro = max(activeMaxGyro, sample.gyroMag);
      recordingCount++;
    }

    unsigned long throwDuration = millis() - throwStartTime;

    if (throwDuration > MIN_THROW_MS) {
      if (sample.gyroMag < STOP_GYRO_THRESHOLD) {
        if (!inLowSpin) {
          inLowSpin = true;
          lowSpinStartTime = millis();
        } else if (millis() - lowSpinStartTime > STOP_DEBOUNCE_MS) {
          finishThrowRecording(throwDuration);
        }
      } else {
        inLowSpin = false;
      }
    }

    if (recording && throwDuration > MAX_THROW_MS) {
      finishThrowRecording(throwDuration);
    }
  }

  if (!recording && queueCount > 0) {
    handleAsyncUpload();
  }

  checkAckTimeout();
}
