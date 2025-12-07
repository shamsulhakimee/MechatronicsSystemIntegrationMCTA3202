#include <BluetoothSerial.h>
#include <DHT.h>
#include <ESP32Servo.h>

// --- Configuration ---
#define DHTPIN 5        // DHT11 data pin connected to ESP32 D5
#define DHTTYPE DHT11   // DHT 11 sensor
#define SERVO_PIN 18    // Servo connected to ESP32 D18
#define BLUETOOTH_NAME "ESP32_FAN_MONITOR" 

// --- Object Initialization ---
BluetoothSerial SerialBT;
DHT dht(DHTPIN, DHTTYPE);
Servo fanServo;

// --- State Variables ---
bool isFanOn = false; // Tracks the state of the fan
unsigned long lastMoveTime = 0; // Time of the last servo movement
const int moveInterval = 10;    // Delay in milliseconds between servo steps
int currentAngle = 0;           // Current servo angle (0 to 180)
bool increasing = true;         // Direction of servo movement

void setup() {
  // Use a fast baud rate for the USB Serial Monitor
  Serial.begin(115200);        
  SerialBT.begin(BLUETOOTH_NAME); 
  dht.begin();
  
  // Attach the servo object to the defined pin
  fanServo.setPeriodHertz(50);
  fanServo.attach(SERVO_PIN, 500, 2400); 
  fanServo.write(0); // Initialize servo to 0 degrees

  Serial.println("=========================================");
  Serial.println("DHT11, Servo, and Bluetooth Monitoring Ready");
  Serial.println("Connect via Bluetooth and send 'O' (ON) or 'X' (OFF).");
  Serial.println("=========================================");
}

void loop() {
  // 1. --- Read and Display Sensor Data (Every 2 seconds) ---
  static unsigned long lastReadTime = 0;
  if (millis() - lastReadTime >= 2000) {
    lastReadTime = millis();
    
    float hum = dht.readHumidity();
    float temp = dht.readTemperature(); 
    
    Serial.println("---");
    if (isnan(hum) || isnan(temp)) {
      Serial.println("🌡️ FAILED to read from DHT sensor!");
    } else {
      // Display the DHT11 sensor data (what you wanted the Python script to show)
      Serial.print("🌡️ DHT11 Reading: Temp = ");
      Serial.print(temp, 2); // Print with 2 decimal places
      Serial.print(" °C, Hum = ");
      Serial.print(hum, 2);
      Serial.println(" %");
      Serial.print("💨 Fan Status: ");
      Serial.println(isFanOn ? "ON" : "OFF");
    }
  }

  // 2. --- Receive and Process Bluetooth Commands ---
  if (SerialBT.available()) {
    // Read the incoming command
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim(); // Remove any leading/trailing whitespace

    // Display the input received via Bluetooth (what you wanted the Python script to show)
    Serial.print("Received Bluetooth Input: '");
    Serial.print(cmd);
    Serial.println("'");
    
    // Command Logic
    if (cmd.equalsIgnoreCase("O")) {
      isFanOn = true;
      // Display FAN ON status (what you wanted the Python script to show)
      Serial.println("✅ Command Feedback: FAN ON"); 
    } else if (cmd.equalsIgnoreCase("X")) {
      isFanOn = false;
      fanServo.write(0); // Set servo to a fixed position when off
      // Display FAN OFF status (what you wanted the Python script to show)
      Serial.println("✅ Command Feedback: FAN OFF"); 
    } else {
      Serial.println("⚠️ Unrecognized command. Send 'O' (ON) or 'X' (OFF)");
    }
  }
  
  // 3. --- Control Servo Sweep (if Fan is ON) ---
  if (isFanOn) {
    if (millis() - lastMoveTime >= moveInterval) {
      lastMoveTime = millis();
      
      // Sweep logic (0 -> 180 -> 0)
      if (increasing) {
        currentAngle++;
        if (currentAngle >= 180) {
          currentAngle = 180;
          increasing = false;
        }
      } else {
        currentAngle--;
        if (currentAngle <= 0) {
          currentAngle = 0;
          increasing = true;
        }
      }
      
      fanServo.write(currentAngle);
    }
  }
}