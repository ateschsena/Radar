const int TRIG_PIN = 8;
const int ECHO_PIN = 9;

long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long us = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  if (us == 0) return -1;                            // no echo
  return (long)(us / 58);                            // cm
}

void setup() {
  Serial.begin(9600);               // set Serial Monitor to 9600
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("BOOT");
  Serial.println("RADAR_READY");
}

void loop() {
  long cm = readDistanceCm();
  Serial.println(cm);               // one value per line = stable plotting
  delay(70);                        // >= 60ms recommended
}
