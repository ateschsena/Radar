int URECHO = 3;   // Arduino D3 -> URM37 Pin 4 ECHO
int URTRIG = 5;   // Arduino D5 -> URM37 Pin 6 COMP/TRIG

void setup() {
  Serial.begin(9600);
  pinMode(URTRIG, OUTPUT);
  digitalWrite(URTRIG, HIGH);
  pinMode(URECHO, INPUT);
  delay(500);
  Serial.println("Init");
}

void loop() {
  // Trigger: kurzer LOW-Puls startet Messung
  digitalWrite(URTRIG, LOW);
  delayMicroseconds(50);
  digitalWrite(URTRIG, HIGH);

  unsigned long t = pulseIn(URECHO, LOW, 60000UL); // low-level pulse
  if (t == 0) {
    Serial.println("0 (kein Puls -> wiring/mode)");
  } else if (t >= 50000) {
    Serial.println("Invalid");
  } else {
    Serial.print("cm=");
    Serial.println(t / 50); // 50us = 1cm
  }

  delay(200);
}