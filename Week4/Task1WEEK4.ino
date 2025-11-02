

#include <Wire.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>


Adafruit_MPU6050 mpu;


void setup() {
  Serial.begin(9600);
 
  // Init I2C (for MPU6050)
  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("MPU6050 connection failed! Check wiring.");
    while (1); // Stop here
  }
 
  // Set MPU ranges
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
 
  Serial.println("MPU6050 Ready. Sending accelerometer data...");
}


void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);


  // Send only the 3 accelerometer axes, as per the PDF's example
  Serial.print(a.acceleration.x);
  Serial.print(",");
  Serial.print(a.acceleration.y);
  Serial.print(",");
  Serial.print(a.acceleration.z);
  Serial.println();
 
  delay(100); // Send data 10 times per second
}


