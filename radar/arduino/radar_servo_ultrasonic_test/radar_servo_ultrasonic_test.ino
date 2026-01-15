#include <Servo.h>

// =======================
// PIN CONFIGURATION
// =======================
const int SERVO_PIN = 9;   // Servo signal pin
const int URTRIG    = 5;   // Arduino D5 -> URM37 Pin 6 COMP/TRIG
const int URECHO    = 3;   // Arduino D3 -> URM37 Pin 4 ECHO

// =======================
// OBJECTS & VARIABLES
// =======================
Servo radarServo;

// Adjust these if you want different scan behavior
const int MIN_ANGLE = 10;
const int MAX_ANGLE = 170;
const int STEP_ANGLE = 2;

const int SERVO_SETTLE_MS = 30;   // time to let servo reach the angle
const int BETWEEN_READ_MS = 10;   // small gap between trigger and read (optional)

// =======================
// SETUP
// =======================
void setup() {
  radarServo.attach(SERVO_PIN);

  pinMode(URTRIG, OUTPUT);
  pinMode(URECHO, INPUT);

  // URM37 idle state: HIGH
  digitalWrite(URTRIG, HIGH);

  Serial.begin(9600);
  delay(500);

  Serial.println("RADAR SYSTEM READY (URM37 mode)");
  Serial.println("Output format: angle,cm  (cm = -1 means out of range/no pulse)");
}

// =======================
// MAIN LOOP
// =======================
void loop() {
  // Sweep forward
  for (int angle = MIN_ANGLE; angle <= MAX_ANGLE; angle += STEP_ANGLE) {
    scanStep(angle);
  }

  // Sweep backward
  for (int angle = MAX_ANGLE; angle >= MIN_ANGLE; angle -= STEP_ANGLE) {
    scanStep(angle);
  }
}

// =======================
// ONE SCAN STEP
// =======================
void scanStep(int angle) {
  radarServo.write(angle);
  delay(SERVO_SETTLE_MS);

  int cm = measureDistanceURM37();

  // Print as "angle,cm" (easy to parse in Serial Plotter / Processing)
  Serial.print(angle);
  Serial.print(",");
  Serial.println(cm);

  delay(BETWEEN_READ_MS);
}

// =======================
// DISTANCE MEASUREMENT (URM37-style)
// - Trigger: short LOW pulse, then HIGH
// - Echo: LOW-level pulse width
// - Conversion: 50 us = 1 cm  => cm = t / 50
// =======================
int measureDistanceURM37() {
  // Start measurement: short LOW pulse
  digitalWrite(URTRIG, LOW);
  delayMicroseconds(50);
  digitalWrite(URTRIG, HIGH);

  // Read LOW pulse width (timeout 60ms)
  unsigned long t = pulseIn(URECHO, LOW, 60000UL);

  if (t == 0) {
    // no pulse -> out of range / wiring / wrong mode
    return -1;
  }

  // Some example codes treat very large values as invalid
  if (t >= 50000UL) {
    return -1;
  }

  // Convert to cm (integer)
  int cm = (int)(t / 50UL);

  // Optional sanity clamp
  if (cm <= 0 || cm > 500) return -1;

  return cm;
}
