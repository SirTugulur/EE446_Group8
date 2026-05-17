#include <Arduino_BMI270_BMM150.h>

// simple code to read 3 positional axes
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(200);
  
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while(1);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  float ax, ay, az // acceleration
  float gx, gy, gz // gyroscope
  float mx, my, mz // magnetic field

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax,ay,az)
  }

  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(gx,gy,gz)
  }

  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx,my,mz);
  }

  delay(50);  
}
