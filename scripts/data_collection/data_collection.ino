#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>

// ---------- CONFIG ----------
const float THROW_ACCEL_THRESHOLD = 2.5;   // g
const float THROW_GYRO_THRESHOLD  = 150.0; // deg/sec
const float CATCH_ACCEL_THRESHOLD = 4.0;   // impact spike

const unsigned long MAX_THROW_MS = 4000;   // Max flight length capped at 4s for RAM safety
const unsigned long MIN_THROW_MS = 300;
const int PRE_TRIGGER_SAMPLES = 30;        // ~150 ms at 5ms/sample

// ---------- STORAGE CONFIG ----------
const int MAX_STORAGE_SAMPLES = 800;       // 800 samples * 5ms delay = 4000ms max window

// ---------- SAMPLE STRUCT ----------
struct IMUSample {
  unsigned long t;
  float ax, ay, az;
  float gx, gy, gz;
  float mx, my, mz;
  float accelMag;
  float gyroMag;
};

// ---------- MEMORY BUFFERS ----------
IMUSample preBuffer[PRE_TRIGGER_SAMPLES];
int preIndex = 0;

IMUSample storageBuffer[MAX_STORAGE_SAMPLES]; // Holds flight data mid-air
int storageCount = 0;

// ---------- THROW STATE ----------
bool recording = false;
unsigned long throwStartTime = 0;
unsigned long throwID = 0;
bool bleActive = false; // Add this line to track the radio state

// ---------- BLE NORDIC UART UUIDS ----------
BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic txCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 255);
BLECharacteristic rxCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, 255);

// =====================================================
// Utility Functions
// =====================================================

float magnitude3(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

// Formats a single structural sample into a lightweight CSV row string
String formatSampleCSV(unsigned long id, IMUSample s) {
  return String(id) + "," + String(s.t) + "," +
         String(s.ax, 4) + "," + String(s.ay, 4) + "," + String(s.az, 4) + "," +
         String(s.gx, 2) + "," + String(s.gy, 2) + "," + String(s.gz, 2) + "," +
         String(s.mx, 1) + "," + String(s.my, 1) + "," + String(s.mz, 1) + "," +
         String(s.accelMag, 4) + "," + String(s.gyroMag, 2) + "\n";
}

// Sends the entire CSV row as a single large BLE packet
void blePrint(String msg) {
  // Write the whole string at once up to our new 255 byte limit
  txCharacteristic.writeValue((const uint8_t*)msg.c_str(), msg.length());
  
  // A slightly longer delay gives the Bluetooth radio time 
  // to clear the larger packet from its buffer
  delay(15);
}

// =====================================================
// Setup
// =====================================================

void setup() {
  delay(2000);

  if (!IMU.begin()) {
    while (1); // Halt if IMU fails
  }

  if (!BLE.begin()) {
    while (1); // Halt if Bluetooth fails
  }

  // Configure BLE Profile
  BLE.setLocalName("FrisbeeTrack");
  BLE.setAdvertisedService(uartService);
  uartService.addCharacteristic(txCharacteristic);
  uartService.addCharacteristic(rxCharacteristic);
  BLE.addService(uartService);
}

// =====================================================
// Main Loop
// =====================================================

void loop() {
  IMUSample sample;

  // ---------- READ ALL IMU DATA ----------
  if (IMU.accelerationAvailable()) IMU.readAcceleration(sample.ax, sample.ay, sample.az);
  if (IMU.gyroscopeAvailable())    IMU.readGyroscope(sample.gx, sample.gy, sample.gz);
  if (IMU.magneticFieldAvailable()) IMU.readMagneticField(sample.mx, sample.my, sample.mz);

  sample.t = micros();
  sample.accelMag = magnitude3(sample.ax, sample.ay, sample.az);
  sample.gyroMag = magnitude3(sample.gx, sample.gy, sample.gz);

  // =====================================================
  // PRE-TRIGGER CIRCULAR BUFFER
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
  if (!recording && storageCount == 0 &&
      sample.accelMag > THROW_ACCEL_THRESHOLD &&
      sample.gyroMag > THROW_GYRO_THRESHOLD) {

    recording = true;
    throwStartTime = millis();
    throwID++;
    storageCount = 0;

    // Offload pre-trigger records to storage array
    int idx = preIndex;
    for (int i = 0; i < PRE_TRIGGER_SAMPLES; i++) {
      if (storageCount < MAX_STORAGE_SAMPLES) {
        storageBuffer[storageCount] = preBuffer[idx];
        storageCount++;
      }
      idx++;
      if (idx >= PRE_TRIGGER_SAMPLES) idx = 0;
    }
  }

  // =====================================================
  // ACTIVE RECORDING (IN-FLIGHT)
  // =====================================================
  if (recording) {
    if (storageCount < MAX_STORAGE_SAMPLES) {
      storageBuffer[storageCount] = sample;
      storageCount++;
    }

    unsigned long throwDuration = millis() - throwStartTime;
    bool catchDetected = (throwDuration > 150 && sample.accelMag > CATCH_ACCEL_THRESHOLD);
    bool timeoutDetected = (throwDuration > MAX_THROW_MS);

    if (catchDetected || timeoutDetected) {
      recording = false;
      // Flight finishes. The data safely rests in storageBuffer waiting for sync.
      Serial.println("Flight finished");
    }
  }

  // =====================================================
  // BLUETOOTH SYNC HANDLING
  // =====================================================
  // Runs only if a complete throw dataset is frozen in memory
  if (!recording && storageCount > 0) {
    
    // Only trigger the advertise command once
    if (!bleActive) {
      BLE.advertise(); 
      bleActive = true;
    }
    
    BLEDevice central = BLE.central();
    if (central) {
      
      // -> THE FIX: Wait right here until the user taps "Subscribe" in the app
      while (central.connected() && !txCharacteristic.subscribed()) {
        delay(10); 
      }

      // If they subscribed, dump the data!
      if (central.connected() && txCharacteristic.subscribed()) {
        
        // 1. Send CSV Headers
        blePrint("throw_id,time_us,ax,ay,az,gx,gy,gz,mx,my,mz,accel_mag,gyro_mag\n");
        
        // 2. Dump sequential arrays
        for (int i = 0; i < storageCount; i++) {
          blePrint(formatSampleCSV(throwID, storageBuffer[i]));
        }
        
        // 3. Send Metadata tail
        blePrint("# METADATA,{\"throw_id\":" + String(throwID) + ",\"samples\":" + String(storageCount) + "}\n\n");
        
        // 4. Clear the memory for the next throw
        storageCount = 0; 
        
        // -> THE NEW FIX: Keep the line open! 
        // The Arduino will idle right here until YOU tap disconnect in the app.
        while (central.connected()) {
          delay(100); 
        }
        
        // Once you disconnect, the board safely shuts down the radio
        BLE.stopAdvertise();
        bleActive = false;
      }
    }
  } else {
    // Turn off radio while tracking flight to save resources/power
    if (bleActive) {
      BLE.stopAdvertise();
      bleActive = false;
    }
  }
}
