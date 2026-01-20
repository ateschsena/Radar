import processing.serial.*;

// ====== CONFIG (easy to change) ======
int BAUD = 9600;

// If you're still sending only "distance", UI simulates sweep:
boolean SIMULATE_ANGLE_IF_MISSING = true;
int SIM_ANGLE_MIN = 0;
int SIM_ANGLE_MAX = 180;
float SIM_ANGLE_STEP = 1;      // degrees per frame/update

// Distance mapping:
float MAX_CM = 300;            // scale radar to 0..MAX_CM cm
float RADIUS_PX = 330;         // size of radar on screen

// ===== Persistence (dot stays until overwritten, but fades over time) =====
int ANGLE_BUCKET_DEG = 2;      // 1 = every degree, 2 = less noisy
int MIN_ALPHA = 120;           // never fade below this (0..255)
int FADE_MS = 2000;            // time until dot reaches MIN_ALPHA (ms)

// ====== state ======
Serial port;
float angleDeg = 0;
float distanceCm = -1;

float simAngle = 0;
float simDir = 1;

// Stores one dot per angle bucket
Ping[] latest = new Ping[181];

// Each stored dot remembers: angle bucket, distance, and when it was last updated
class Ping {
  float a, d;
  int bornMs;
  Ping(float a, float d, int bornMs) { this.a = a; this.d = d; this.bornMs = bornMs; }
}

void setup() {
  size(900, 520);
  smooth();
  frameRate(60);

  println("Available serial ports:");
  printArray(Serial.list());

  port = new Serial(this, "COM7", BAUD); // replace with your Arduino COM
  port.bufferUntil('\n'); // read line-by-line

  angleDeg = 0;
  simAngle = 0;
}

void draw() {
  background(0);

  // Simulated sweep if we don't receive angle from serial
  if (SIMULATE_ANGLE_IF_MISSING) {
    simAngle += simDir * SIM_ANGLE_STEP;

    if (simAngle >= SIM_ANGLE_MAX) { simAngle = SIM_ANGLE_MAX; simDir = -1; }
    if (simAngle <= SIM_ANGLE_MIN) { simAngle = SIM_ANGLE_MIN; simDir =  1; }

    if (!lastLineHadAngle) angleDeg = simAngle;
  }

  // Radar origin
  float cx = width * 0.5;
  float cy = height * 0.92;

  drawGrid(cx, cy);
  drawSweepAndUpdateBucket(cx, cy, angleDeg);  // <-- sweep overwrites bucket every time
  drawPersistentDots(cx, cy);

  // UI text
  textAlign(LEFT, TOP);

  fill(0, 255, 0);
  textSize(16);
  text("Angle: " + int(angleDeg) + "°", 20, 30);
  text("Distance: " + int(distanceCm) + " cm", 20, 55);
  text("Mode: " + (lastLineHadAngle ? "REAL angle from Serial" : "SIMULATED angle"), 20, 80);
}

void drawGrid(float cx, float cy) {
  stroke(0, 200, 0);
  strokeWeight(2);
  noFill();

  // Range arcs + labels
  for (int i = 1; i <= 4; i++) {
    float r = (RADIUS_PX / 4) * i;
    arc(cx, cy, r * 2, r * 2, PI, TWO_PI);
  
    float dist = (MAX_CM / 4.0) * i;
  
    fill(0, 200, 0);
    textSize(14);
    textAlign(CENTER, BOTTOM);
    text(fmtCm(dist), cx, cy - r - 8);
  
    noFill();
  }


  // Baseline
  line(cx - RADIUS_PX, cy, cx + RADIUS_PX, cy);

  // Angle divider lines
  strokeWeight(1);
  for (int a = 0; a <= 180; a += 30) {
    float rad = radians(a);
    float x = cx + cos(rad) * RADIUS_PX;
    float y = cy - sin(rad) * RADIUS_PX;
    line(cx, cy, x, y);
  }
}


void drawSweepAndUpdateBucket(float cx, float cy, float angDeg) {
  // Draw sweep line
  stroke(255, 0, 0);
  strokeWeight(3);

  float rad = radians(angDeg);
  float x = cx + cos(rad) * RADIUS_PX;
  float y = cy - sin(rad) * RADIUS_PX;
  line(cx, cy, x, y);

  // --- IMPORTANT CHANGE ---
  // Every time the sweep passes an angle bucket, overwrite it.
  int aInt = constrain(round(angDeg), 0, 180);
  aInt = (aInt / ANGLE_BUCKET_DEG) * ANGLE_BUCKET_DEG;

  boolean valid = (distanceCm > 0 && distanceCm <= MAX_CM);

  if (valid) {
    // new detection -> replace old dot and reset fade
    latest[aInt] = new Ping(aInt, distanceCm, millis());
  } else {
    // nothing detected / out of range -> sweep clears old dot at this angle
    latest[aInt] = null;
  }
}

void drawPersistentDots(float cx, float cy) {
  noStroke();
  int now = millis();

  for (int a = 0; a <= 180; a += ANGLE_BUCKET_DEG) {
    Ping p = latest[a];
    if (p == null) continue;

    // fade immediately after spawn, down to MIN_ALPHA over FADE_MS
    float age01 = constrain((now - p.bornMs) / (float)FADE_MS, 0, 1);

    // optional easing (comment out if you want linear fade)
    float eased = 1 - pow(1 - age01, 3);

    float alpha = lerp(255, MIN_ALPHA, eased);

    float rr = map(p.d, 0, MAX_CM, 0, RADIUS_PX);
    float rad = radians(p.a);
    float x = cx + cos(rad) * rr;
    float y = cy - sin(rad) * rr;

    // Trail is RED
    fill(255, 0, 0, alpha);
    circle(x, y, 7);
  }
}

// Tracks whether last serial line contained angle,distance
boolean lastLineHadAngle = false;

void serialEvent(Serial p) {
  String line = p.readStringUntil('\n');
  if (line == null) return;
  line = trim(line);

  // Ignore handshake/noise
  if (line.length() == 0) return;
  if (line.equals("BOOT")) return;
  if (line.equals("RADAR_READY")) return;

  // Two formats supported:
  // 1) "angle,distance"
  // 2) "distance"
  String[] parts = split(line, ',');

  try {
    if (parts.length == 2) {
      float a = float(parts[0]);
      float d = float(parts[1]);
      if (a < 0 || a > 180) return;

      angleDeg = a;
      distanceCm = d;
      lastLineHadAngle = true;

    } else if (parts.length == 1) {
      float d = float(parts[0]);
      distanceCm = d;
      lastLineHadAngle = false;
    }
  } catch(Exception e) {
    // ignore parsing errors
  }
}
  String fmtCm(float v) {
  return int(round(v)) + " cm";
}
