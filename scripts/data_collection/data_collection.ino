#include <Arduino_BMI270_BMM150.h>

// =====================================================
// Frisbee Throw Data Collector
// =====================================================

// ---------- CONFIG ----------
const float THROW_ACCEL_THRESHOLD = 2.5;   // g
const float THROW_GYRO_THRESHOLD  = 150.0; // deg/sec

const float CATCH_ACCEL_THRESHOLD = 4.0;   // impact spike

const unsigned long MAX_THROW_MS = 5000;
const unsigned long MIN_THROW_MS = 300;

const int PRE_TRIGGER_SAMPLES = 30; // ~150 ms at 5ms/sample

// ---------- SAMPLE STRUCT ----------
struct IMUSample {
  unsigned long t;

  float ax, ay, az;
  float gx, gy, gz;
  float mx, my, mz;

  float accelMag;
  float gyroMag;
};

// ---------- PRE-TRIGGER BUFFER ----------
IMUSample preBuffer[PRE_TRIGGER_SAMPLES];
int preIndex = 0;

// ---------- THROW STATE ----------
bool recording = false;

unsigned long throwStartTime = 0;
unsigned long throwID = 0;

// =====================================================
// Utility Functions
// =====================================================

float magnitude3(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

void printCSVHeader() {
  Serial.println(
    "throw_id,time_us,ax,ay,az,gx,gy,gz,mx,my,mz,accel_mag,gyro_mag"
  );
}

void printSampleCSV(unsigned long id, IMUSample s) {
  Serial.print(id);
  Serial.print(",");

  Serial.print(s.t);
  Serial.print(",");

  Serial.print(s.ax, 5);
  Serial.print(",");
  Serial.print(s.ay, 5);
  Serial.print(",");
  Serial.print(s.az, 5);
  Serial.print(",");

  Serial.print(s.gx, 5);
  Serial.print(",");
  Serial.print(s.gy, 5);
  Serial.print(",");
  Serial.print(s.gz, 5);
  Serial.print(",");

  Serial.print(s.mx, 5);
  Serial.print(",");
  Serial.print(s.my, 5);
  Serial.print(",");
  Serial.print(s.mz, 5);
  Serial.print(",");

  Serial.print(s.accelMag, 5);
  Serial.print(",");

  Serial.println(s.gyroMag, 5);
}

// =====================================================
// Setup
// =====================================================

void setup() {
  Serial.begin(115200);

  delay(2000);

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  printCSVHeader();

  Serial.println("# READY");
}

// =====================================================
// Main Loop
// =====================================================

void loop() {

  IMUSample sample;

  // ---------- READ ALL IMU DATA ----------
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(sample.ax, sample.ay, sample.az);
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(sample.gx, sample.gy, sample.gz);
  }

  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(sample.mx, sample.my, sample.mz);
  }

  // ---------- TIMESTAMP ----------
  sample.t = micros();

  // ---------- MAGNITUDES ----------
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
  // PRE-TRIGGER CIRCULAR BUFFER
  // =====================================================

  preBuffer[preIndex] = sample;
  preIndex++;

  if (preIndex >= PRE_TRIGGER_SAMPLES) {
    preIndex = 0;
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

    Serial.println("# THROW_START");

    // ---------- Dump pre-trigger buffer ----------
    int idx = preIndex;

    for (int i = 0; i < PRE_TRIGGER_SAMPLES; i++) {

      printSampleCSV(throwID, preBuffer[idx]);

      idx++;

      if (idx >= PRE_TRIGGER_SAMPLES) {
        idx = 0;
      }
    }
  }

  // =====================================================
  // RECORDING
  // =====================================================

  if (recording) {

    printSampleCSV(throwID, sample);

    unsigned long throwDuration =
      millis() - throwStartTime;

    // =========================================
    // Catch / impact detection
    // =========================================

    bool catchDetected =
      (throwDuration > 150 &&
       sample.accelMag > CATCH_ACCEL_THRESHOLD);

    // =========================================
    // Timeout stop
    // =========================================

    bool timeoutDetected =
      (throwDuration > MAX_THROW_MS);

    // =========================================
    // Stop Conditions
    // =========================================

    if (catchDetected || timeoutDetected) {

      recording = false;

      Serial.println("# THROW_END");

      // =====================================
      // Metadata
      // =====================================

      Serial.print("# METADATA,");

      Serial.print("{");

      Serial.print("\"throw_id\":");
      Serial.print(throwID);
      Serial.print(",");

      Serial.print("\"throw_type\":\"unknown\",");
      Serial.print("\"notes\":\"\",");
      Serial.print("\"sampling_rate_hz\":200,");
      Serial.print("\"duration_ms\":");
      Serial.print(throwDuration);

      Serial.println("}");

      // =====================================
      // Failed throw detection
      // =====================================

      if (throwDuration < MIN_THROW_MS) {
        Serial.println("# FAILED_THROW");
      }

      Serial.println();
    }
  }

  // ~200 Hz
  delay(5);
}
