// SilentTalk Glove - Sensor Mode with Calibration
// Arduino Uno + HC-05 (SoftwareSerial on 10/11)

#include <SoftwareSerial.h>

SoftwareSerial BT(10, 11); // RX, TX to HC-05

const int flexPins[4] = {A0, A1, A2, A3}; // index,middle,ring,little
const int sosPin = 2;
const int buzzerPin = 6;

int flexRaw[4];
int flexSm[4];
const int SMOOTH_N = 6; // smooth samples
int flexBuf[4][SMOOTH_N];
int bufIdx = 0;

int calOpen[4];   // values when hand open
int calFist[4];   // values when fist (all bent)
bool calibrated = false;

unsigned long lastSend = 0;
const unsigned long SEND_COOLDOWN = 700; // ms between messages

void setup() {
  Serial.begin(9600);
  BT.begin(9600);
  pinMode(sosPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  for (int i=0;i<4;i++){
    pinMode(flexPins[i], INPUT);
    for (int j=0;j<SMOOTH_N;j++) flexBuf[i][j] = analogRead(flexPins[i]);
    flexSm[i] = analogRead(flexPins[i]);
  }
  delay(200);
  Serial.println("SilentTalk Glove ready.");
  BT.println("READY");
  calibrateRoutine();
}

void calibrateRoutine() {
  // Instructions for the user (print via Serial)
  Serial.println("Calibration: keep hand OPEN (flat) and press ENTER in Serial Monitor.");
  while (!Serial.available()) { delay(100); } // wait for enter
  delay(300);
  // take open averages
  for (int i=0;i<40;i++){
    for (int f=0;f<4;f++) calOpen[f] = (calOpen[f]*i + analogRead(flexPins[f])) / (i+1);
    delay(40);
  }
  Serial.println("Now make a FIST and press ENTER.");
  while (!Serial.available()) { delay(100); }
  delay(300);
  for (int i=0;i<40;i++){
    for (int f=0;f<4;f++) calFist[f] = (calFist[f]*i + analogRead(flexPins[f])) / (i+1);
    delay(40);
  }
  calibrated = true;
  Serial.println("Calibration done.");
  Serial.print("Open: ");
  for (int i=0;i<4;i++) Serial.print(calOpen[i]), Serial.print(" ");
  Serial.println();
  Serial.print("Fist: ");
  for (int i=0;i<4;i++) Serial.print(calFist[i]), Serial.print(" ");
  Serial.println();
  Serial.println("You can now use the glove. Press SOS button to send emergency.");
}

void smoothRead() {
  bufIdx = (bufIdx + 1) % SMOOTH_N;
  for (int i=0;i<4;i++){
    flexBuf[i][bufIdx] = analogRead(flexPins[i]);
    long s = 0;
    for (int j=0;j<SMOOTH_N;j++) s += flexBuf[i][j];
    flexSm[i] = s / SMOOTH_N;
  }
}

String classifyGesture() {
  // compare smoothed values against calibration range
  // Compute normalized bend ratio 0.0 (open) -> 1.0 (fist)
  float r[4];
  for (int i=0;i<4;i++){
    float denom = max(1, calFist[i] - calOpen[i]);
    r[i] = (flexSm[i] - calOpen[i]) / denom;
    if (r[i] < 0) r[i] = 0;
    if (r[i] > 1) r[i] = 1;
  }
  // thresholds: consider a finger 'bent' if r > 0.6
  bool b0 = r[0] > 0.6; // index
  bool b1 = r[1] > 0.6; // middle
  bool b2 = r[2] > 0.6; // ring
  bool b3 = r[3] > 0.6; // little

  // Example phrase mappings:
  if (!b0 && !b1 && !b2 && !b3) return "Hello";        // open palm
  if (b0 && b1 && b2 && b3) return "Thanks";           // fist
  if (b0 && !b1 && !b2 && !b3) return "Yes";           // index bent (point)
  if (!b0 && b1 && !b2 && !b3) return "No";            // middle bent (thumb substitute)
  if (b0 && b1 && !b2 && !b3) return "I need help";    // index+middle
  if (b0 && !b1 && b2 && !b3) return "Come here";      // index+ring (example)
  return ""; // nothing recognized
}

void loop() {
  smoothRead();
  // SOS check
  if (digitalRead(sosPin) == LOW) {
    sendMessage("SOS|Emergency! Please help me now.");
    feedbackTone(3);
    delay(1500);
    return;
  }

  // classify and send with cooldown
  String g = classifyGesture();
  if (g.length() > 0 && (millis() - lastSend) > SEND_COOLDOWN) {
    sendMessage("G|" + g);
    feedbackTone(1);
    lastSend = millis();
  }
  delay(120);
}

void sendMessage(String msg) {
  BT.println(msg);
  Serial.println("BT-> " + msg);
}

void feedbackTone(int type) {
  // short beep(s)
  for (int i=0;i<type;i++){
    tone(buzzerPin, 2000, 120);
    delay(150);
  }
}
