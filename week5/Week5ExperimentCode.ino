#include <Encoder.h>
// --- MOTOR CONTROL PINS ---
const int MOTOR_PWM_PIN = 5; // PWM pin
const int MOTOR_DIR_PIN = 4; // Direction pin
// --- ENCODER PINS ---
const int ENCODER_A_PIN = 2;
const int ENCODER_B_PIN = 20;
const float COUNTS_PER_REVOLUTION = 60.0; // for SPG30-20K motor with gearbox
Encoder motorEncoder(ENCODER_A_PIN, ENCODER_B_PIN);
// --- VARIABLES FOR RPM CALCULATION ---
long last_position = 0;
unsigned long last_time = 0;
const int INTERVAL_MS = 100;
// --- PWM TEST VALUES (based on your observation table) --- 8
int pwm_values[] = {255, 128, 255, 64};
bool directions[] = {true, true, false, false}; // true = forward, false = reverse
int num_tests = 4;
int current_test = 0;
unsigned long test_start_time = 0;
const unsigned long TEST_DURATION = 5000; // run each test for 5 seconds
// --- For averaging RPM ---
float rpm_sum = 0;
int rpm_count = 0;
void setup() {
Serial.begin(9600);
Serial.println("DC Motor PWM Test with Average RPM Measurement");
Serial.println("-------------------------------------------------");
Serial.println("PWM\tDirection\tAverage RPM");
pinMode(MOTOR_PWM_PIN, OUTPUT);
pinMode(MOTOR_DIR_PIN, OUTPUT);
last_time = millis();
last_position = motorEncoder.read();
startTest(current_test);
}
void loop() {
// --- Measure RPM every INTERVAL_MS ---
if (millis() - last_time >= INTERVAL_MS) {
long current_position = motorEncoder.read();
long delta_counts = abs(current_position - last_position);
unsigned long delta_time_ms = millis() - last_time;
float rpm = (float)delta_counts / COUNTS_PER_REVOLUTION;
rpm = rpm / ((float)delta_time_ms / 60000.0);
last_position = current_position;
last_time = millis();
// Collect data for averaging
rpm_sum += rpm;
rpm_count++;
}
// --- Move to next PWM test after duration ---
if (millis() - test_start_time >= TEST_DURATION) {
// Calculate and print average RPM for this test
float average_rpm = (rpm_count > 0) ? rpm_sum / rpm_count : 0;
Serial.print(pwm_values[current_test]);
Serial.print("\t");
Serial.print(directions[current_test] ? "Forward" : "Reverse");
Serial.print("\t");
Serial.println(average_rpm, 1);
// Prepare for next test
current_test++;
if (current_test < num_tests) {
startTest(current_test);
} else {
Serial.println("-------------------------------------------------");
Serial.println("All tests completed. Motor stopped.");
analogWrite(MOTOR_PWM_PIN, 0);
while (true); // stop program
}
}
}
// --- Function to start each test ---
void startTest(int index) {
Serial.println("-------------------------------------------------");
Serial.print("Starting Test ");
Serial.print(index + 1);
Serial.print(": PWM=");
Serial.print(pwm_values[index]);
Serial.print(", Direction=");
Serial.println(directions[index] ? "Forward" : "Reverse");
// Reset averaging variables
rpm_sum = 0; 10
rpm_count = 0;
// Apply direction and speed
digitalWrite(MOTOR_DIR_PIN, directions[index] ? HIGH : LOW);
analogWrite(MOTOR_PWM_PIN, pwm_values[index]);
// Reset timers
test_start_time = millis();
last_position = motorEncoder.read();
last_time = millis();
}