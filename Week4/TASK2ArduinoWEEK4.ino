/*
  This sketch is for Task 2 (Python-Controlled System).
  It has been updated to only send MPU data when requested by Python.
  It listens for:
  - 'A': Access (Unlock servo, green LED)
  - 'D': Deny (Lock servo, red LED)
  - 'G': Gesture (Start sending MPU data, blink green LED)
*/

// --- Include Libraries ---
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- Pin Definitions (for your MEGA) ---
#define RFID_RST_PIN 9
#define RFID_SS_PIN  10 // SPI SS (The "SDA" pin on the RC522)
#define SERVO_PIN    7
#define LED_GREEN_PIN 5
#define LED_RED_PIN   6
// MPU6050 uses dedicated I2C pins: 20 (SDA) and 21 (SCL)

// --- Create Objects ---
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN); 
Servo lockServo;
Adafruit_MPU6050 mpu;

// --- Global Variables ---
unsigned long lastMpuReadTime = 0;
const long mpuReadInterval = 100; // Read MPU every 100ms
const int OPEN_ANGLE = 180;
const int CLOSED_ANGLE = 0;

bool sendMpuData = false; // Flag to control MPU data streaming
unsigned long lastBlinkTime = 0;
bool greenLedState = false;

void setup() {
  Serial.begin(9600); // Must match Python's baud rate
  
  // Init SPI (for RFID)
  SPI.begin();
  rfid.PCD_Init();

  // Init I2C (for MPU6050)
  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("MPU6050 connection failed! Check wiring.");
    while (1); // Stop here
  }
  
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Init LEDs
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  
  // Set initial LOCKED state
  setLockedState();
  
  // --- CRITICAL FIX ---
  // Flush the serial buffer
  while(Serial.available()) Serial.read();
  
  Serial.println("Arduino is ready. Waiting for Python commands.");
}

void loop() {
  // --- Task 1: Check for Serial Commands FROM Python ---
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    if (command == 'A') { // Access Granted
      setUnlockedState();
      sendMpuData = false; // Stop sending MPU data
      
    } else if (command == 'D') { // Access Denied or Relock
      setLockedState();
      sendMpuData = false; // Stop sending MPU data
      
    } else if (command == 'G') { // Gesture mode
      sendMpuData = true; // Start sending MPU data
      lastBlinkTime = millis(); // Start blinking
      greenLedState = true;
      digitalWrite(LED_GREEN_PIN, greenLedState);
      digitalWrite(LED_RED_PIN, LOW); // Turn red off
    }
  }

  // --- Task 2: Check for RFID Tag (Non-blocking) ---
  // Only scan for RFID if we are not in gesture mode
  if (!sendMpuData) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      Serial.print("UID:"); // Send UID to Python
      for (byte i = 0; i < rfid.uid.size; i++) {
        Serial.print(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        Serial.print(rfid.uid.uidByte[i], HEX);
      }
      Serial.println();
      
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1(); 
    }
  }

  // --- Task 3: Read MPU6050 (if requested) ---
  if (sendMpuData) {
    // Handle blinking green LED
    if (millis() - lastBlinkTime > 300) {
      lastBlinkTime = millis();
      greenLedState = !greenLedState;
      digitalWrite(LED_GREEN_PIN, greenLedState);
    }
    
    // Handle sending MPU data
    if (millis() - lastMpuReadTime > mpuReadInterval) {
      lastMpuReadTime = millis();
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);

      // Send all 6 axes to Python
      Serial.print("MPU:");
      Serial.print(a.acceleration.x); Serial.print(",");
      Serial.print(a.acceleration.y); Serial.print(",");
      Serial.print(a.acceleration.z); Serial.print(",");
      Serial.print(g.gyro.x); Serial.print(",");
      Serial.print(g.gyro.y); Serial.print(",");
      Serial.print(g.gyro.z);
      Serial.println();
    }
  }
}

// --- Helper Functions ---

void setLockedState() {
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, HIGH);
  lockServo.attach(SERVO_PIN);
  lockServo.write(CLOSED_ANGLE);
  delay(500); // Give servo time to move
  lockServo.detach();
  rfid.PCD_Init(); // Re-init RFID reader
}

void setUnlockedState() {
  digitalWrite(LED_GREEN_PIN, HIGH); // Solid Green
  digitalWrite(LED_RED_PIN, LOW);
  lockServo.attach(SERVO_PIN);
  lockServo.write(OPEN_ANGLE);
  delay(500); // Give servo time to move
  lockServo.detach();
}

