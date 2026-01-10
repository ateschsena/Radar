#include <Servo.h>

// PIN CONFIGURATION

// Change only these if your wiring is different
const int SERVO_PIN = 6;    // Servo signal pin
const int TRIG_PIN  = 9;    // HC-SR04 TRIG pin
const int ECHO_PIN  = 10;   // HC-SR04 ECHO pin


// OBJECTS & VARIABLES

Servo radarServo;

long duration;   // Time of ultrasonic pulse
int distance;    // Calculated distance in cm


// SETUP

void setup() {
  // Attach servo to its signal pin
  radarServo.attach(SERVO_PIN);

  // Configure ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Make sure TRIG is LOW at start
  digitalWrite(TRIG_PIN, LOW);

  // Start serial communication
  Serial.begin(9600);

  // Give hardware time to stabilize
  delay(1000);

  Serial.println("RADAR SYSTEM READY");
}


// MAIN LOOP

void loop() {

  // Sweep from 0 to 180 degrees
  for (int angle = 0; angle <= 180; angle += 2) {
    radarServo.write(angle);
    delay(30); // Slow movement for stability

    distance = measureDistance();
    printData(angle, distance);
  }

  // Sweep back from 180 to 0 degrees
  for (int angle = 180; angle >= 0; angle -= 2) {
    radarServo.write(angle);
    delay(30);

    distance = measureDistance();
    printData(angle, distance);
  }
}


// DISTANCE MEASUREMENT

int measureDistance() {
  // Send ultrasonic trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo time (timeout = 30 ms)
  duration = pulseIn(ECHO_PIN, HIGH, 30000);

  // If no echo received
  if (duration == 0) return -1;

  // Convert time to distance (cm)
  return duration * 0.034 / 2;
}


// SERIAL OUTPUT

void printData(int angle, int dist) {
  Serial.print(angle);
  Serial.print(",");

  // Filter invalid or out-of-range values
  if (dist > 0 && dist < 400) {
    Serial.println(dist);
  } else {
    Serial.println(0);
  }
}
