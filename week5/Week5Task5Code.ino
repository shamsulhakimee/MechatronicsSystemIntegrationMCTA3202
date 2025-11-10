#include <Encoder.h>

// --- MOTOR CONTROL PINS ---
const int MOTOR_PWM_PIN = 5;  // PWM pin
const int MOTOR_DIR_PIN = 4;  // Direction pin

// --- ENCODER PINS ---
const int ENCODER_A_PIN = 2;
const int ENCODER_B_PIN = 20;
const float COUNTS_PER_REVOLUTION = 60.0;  // SPG30-20K (3*20)

// --- OBJECTS & VARIABLES ---
Encoder motorEncoder(ENCODER_A_PIN, ENCODER_B_PIN);

long last_position = 0;
unsigned long last_time = 0;
const int INTERVAL_MS = 100;

int target_pwm = 0;
bool is_forward = true;  // true = forward, false = reverse

void setup() {
  Serial.begin(9600);
  Serial.println("DC Motor Manual Control (Forward / Reverse + RPM Feedback)");
  Serial.println("-------------------------------------------------------------");
  Serial.println("Commands:");
  Serial.println("  f <pwm>   -> Forward at PWM (0–255)");
  Serial.println("  r <pwm>   -> Reverse at PWM (0–255)");
  Serial.println("  s         -> Stop motor");
  Serial.println("-------------------------------------------------------------");

  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);

  analogWrite(MOTOR_PWM_PIN, 0);
  digitalWrite(MOTOR_DIR_PIN, LOW);

  last_time = millis();
  last_position = motorEncoder.read();
}

void loop() {
  // --- READ SERIAL COMMANDS ---
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("f")) {  // Forward
      int pwm_value = command.substring(1).toInt();
      if (pwm_value < 0) pwm_value = 0;
      if (pwm_value > 255) pwm_value = 255;
      is_forward = true;
      target_pwm = pwm_value;
      digitalWrite(MOTOR_DIR_PIN, HIGH);
      analogWrite(MOTOR_PWM_PIN, target_pwm);
      Serial.print("→ Motor running FORWARD at PWM=");
      Serial.println(target_pwm);
    }
    else if (command.startsWith("r")) {  // Reverse
      int pwm_value = command.substring(1).toInt();
      if (pwm_value < 0) pwm_value = 0;
      if (pwm_value > 255) pwm_value = 255;
      is_forward = false;
      target_pwm = pwm_value;
      digitalWrite(MOTOR_DIR_PIN, LOW);
      analogWrite(MOTOR_PWM_PIN, target_pwm);
      Serial.print("→ Motor running REVERSE at PWM=");
      Serial.println(target_pwm);
    }
    else if (command.startsWith("s")) {  // Stop
      analogWrite(MOTOR_PWM_PIN, 0);
      target_pwm = 0;
      Serial.println("→ Motor stopped.");
    }
    else {
      Serial.println("Invalid command! Use: f<value>, r<value>, or s");
    }
  }

  // --- CALCULATE AND PRINT RPM EVERY 100 ms ---
  if (millis() - last_time >= INTERVAL_MS) {
    long current_position = motorEncoder.read();
    long delta_counts = abs(current_position - last_position);
    unsigned long delta_time_ms = millis() - last_time;

    float rpm = (float)delta_counts / COUNTS_PER_REVOLUTION;
    rpm = rpm / ((float)delta_time_ms / 60000.0);

    last_position = current_position;
    last_time = millis();

    // Display live feedback
    Serial.print("Direction: ");
    Serial.print(is_forward ? "Forward" : "Reverse");
    Serial.print(" | PWM: ");
    Serial.print(target_pwm);
    Serial.print(" | RPM: ");
    Serial.println(rpm, 1);
  }
}
