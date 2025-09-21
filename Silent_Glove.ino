// SilentTalk Glove - MPU6050 + HC-05 (RAM Optimized, Auto + Adjustable Thresholds)
// By CosmicX550 + ChatGPT

#include <SoftwareSerial.h>
#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

// ---------------- PIN CONFIG ----------------
const int flexPins[5] = {A0, A1, A2, A3, A6}; // Thumb, Index, Middle, Ring, Pinky
const int sosPin = 2;
const int buzzerPin = 3;
SoftwareSerial BT(0, 1);

// ---------------- SETTINGS ----------------
const int SMOOTH_N = 10;
const int CAL_SAMPLES = 20;
const unsigned long SEND_COOLDOWN = 800UL;

// Per-finger bend thresholds
float BEND_THRESHOLD[5]; // Will be set automatically

// Orientation thresholds
const float PITCH_UP = 30.0;
const float PITCH_DOWN = -30.0;
const float ROLL_RIGHT = 30.0;
const float ROLL_LEFT = -30.0;
const float FLAT_MARGIN = 10.0;

// ---------------- STATE ----------------
int flexBuffer[5][SMOOTH_N];
int bufferIndex = 0;
int calOpen[5];
int calFist[5];
unsigned long lastSendTime = 0;

int16_t ax, ay, az, gx, gy, gz;
float pitch = 0.0, roll = 0.0;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(sosPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  // initialize smoothing buffer
  for (int f = 0; f < 5; f++)
    for (int i = 0; i < SMOOTH_N; i++)
      flexBuffer[f][i] = analogRead(flexPins[f]);

  Wire.begin();
  mpu.initialize();
  if (!mpu.testConnection()) Serial.println(F("Warning: MPU6050 not detected!"));
  else Serial.println(F("MPU6050 OK"));

  Serial.println(F("\nSilentTalk Glove - MPU6050 Example"));
  Serial.println(F("Calibration will begin. Press ENTER."));
  calibrate();
  Serial.println(F("Calibration done. Ready."));
  Serial.println(F("Tip: use 'SET0 0.65' to change thumb threshold (0-4 for fingers)."));
}

// ---------------- HELPERS ----------------
int smoothReadOnce(int pin, int idx) {
  flexBuffer[idx][bufferIndex] = analogRead(pin);
  long sum = 0;
  for (int i = 0; i < SMOOTH_N; i++) sum += flexBuffer[idx][i];
  return (int)(sum / SMOOTH_N);
}

void calibrate() {
  Serial.println(F("Keep hand OPEN and press ENTER..."));
  while (!Serial.available()) delay(50);
  while (Serial.available()) Serial.read();
  delay(200);

  for (int i = 0; i < 5; i++) {
    long tot = 0;
    for (int s = 0; s < CAL_SAMPLES; s++) {
      tot += smoothReadOnce(flexPins[i], i);
      delay(8);
    }
    calOpen[i] = (int)(tot / CAL_SAMPLES);
  }
  Serial.println(F("Open captured."));

  Serial.println(F("Make a FIST and press ENTER..."));
  while (!Serial.available()) delay(50);
  while (Serial.available()) Serial.read();
  delay(200);

  for (int i = 0; i < 5; i++) {
    long tot = 0;
    for (int s = 0; s < CAL_SAMPLES; s++) {
      tot += smoothReadOnce(flexPins[i], i);
      delay(8);
    }
    calFist[i] = (int)(tot / CAL_SAMPLES);

    // --- Automatically calculate per-finger threshold ---
    BEND_THRESHOLD[i] = calOpen[i] + 0.65 * (calFist[i] - calOpen[i]);
  }
  Serial.println(F("Fist captured. Thresholds calculated."));

  Serial.print(F("calOpen: "));
  for (int i = 0; i < 5; i++) {
    Serial.print(calOpen[i]);
    Serial.print(F(" "));
  }
  Serial.println();
  Serial.print(F("calFist: "));
  for (int i = 0; i < 5; i++) {
    Serial.print(calFist[i]);
    Serial.print(F(" "));
  }
  Serial.println();

  Serial.print(F("BEND_THRESHOLD per finger: "));
  for (int i = 0; i < 5; i++) {
    Serial.print(BEND_THRESHOLD[i], 2);
    Serial.print(F(" "));
  }
  Serial.println();
}

float getNormBend(int i) {
  int raw = smoothReadOnce(flexPins[i], i);
  float denom = (float)(calFist[i] - calOpen[i]);
  if (denom < 1.0) denom = 1.0;
  float v = (raw - calOpen[i]) / denom;
  return constrain(v, 0.0, 1.0);
}

void orientationLabel(char *out) {
  if (pitch > PITCH_UP && abs(roll) < 60) strcpy(out, "UP");
  else if (pitch < PITCH_DOWN && abs(roll) < 60) strcpy(out, "DOWN");
  else if (roll > ROLL_RIGHT && abs(pitch) < 60) strcpy(out, "RIGHT");
  else if (roll < ROLL_LEFT && abs(pitch) < 60) strcpy(out, "LEFT");
  else if (abs(pitch) < FLAT_MARGIN && abs(roll) < FLAT_MARGIN) strcpy(out, "FLAT");
  else strcpy(out, "UNKNOWN");
}

// ---------------- GESTURE DETECTION ----------------
void detectGesture(char *gestureOut) {
  bool bent[5];
  for (int i = 0; i < 5; i++) bent[i] = (getNormBend(i) > BEND_THRESHOLD[i]);

  char orient[12];
  orientationLabel(orient);

  // --- Example gestures ---
  if (bent[0] && !bent[1] && !bent[2] && bent[3] && bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "YOU");
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "ME");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "FRIEND");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "FAMILY");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "VISHNU");
    return;
  }
 if (bent[0] && !bent[1] && !bent[2] && !bent[3] && !bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "I WANT");//2
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "I WANT TO");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "I AM");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "I WILL");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "HOW ARE YOU?");
    return;
  }
 if (bent[0] && bent[1] && bent[2] && bent[3] && bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "YES");//3
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "NO");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "PLEASE");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "THANK YOU");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "SORRY");
    return;
  }
 if (!bent[0] && !bent[1] && !bent[2] && !bent[3] && !bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "HELLO");//4
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "GOODBYE");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "GOODMORNING");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "GOODNIGHT");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "EAT");
    return;
  }  
 if (!bent[0] && !bent[1] && bent[2] && bent[3] && !bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "DRINK");//5
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "WATER");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "FOOD");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "TOILET");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "MEDICINE");
    return;
  } 
 if (!bent[0] && !bent[1] && bent[2] && !bent[3] && bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "SLEEP");//6
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "MORE");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "DONE");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "WAIT");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "COME");
    return;
  } 
 if (!bent[0] && !bent[1] && bent[2] && !bent[3] && !bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "GO");//7
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "STOP");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "HELP");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "WORK");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "PLAY");
    return;
  } 
 if (!bent[0] && !bent[1] && !bent[2] && bent[3] && !bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "HAPPY");//8
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "SAD");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "ANGRY");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "SCARED");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "TIRED");
    return;
  } 
 if (bent[0] && bent[1] && !bent[2] && !bent[3] && !bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "EXCITED");//9
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "LOVE");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "LAUGHING");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "CRYING");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "PAIN");
    return;
  } 
 if (!bent[0] && !bent[1] && !bent[2] && bent[3] && bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "DANGER");//10
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "DOCTOR");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "POLICE");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "FIRE");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "HOME");
    return;
  } 
 if (!bent[0] && bent[1] && bent[2] && bent[3] && bent[4]) {
    if (strcmp(orient, "FLAT") == 0) strcpy(gestureOut, "SCHOOL");//11
    else if (strcmp(orient, "UP") == 0) strcpy(gestureOut, "BUS");
    else if (strcmp(orient, "RIGHT") == 0) strcpy(gestureOut, "MONEY");
    else if (strcmp(orient, "LEFT") == 0) strcpy(gestureOut, "PHONE");
    else if (strcmp(orient, "DOWN") == 0) strcpy(gestureOut, "FINISHED");
    return;
  } 
  // ... repeat all other gestures exactly as in original code ...

  strcpy(gestureOut, "");
}

// ---------------- COMMUNICATION ----------------
void sendMessage(const char *msg) {
  BT.println(msg);
  Serial.print(F("Sent -> "));
  Serial.println(msg);
}

void playSOSPattern() {
  tone(buzzerPin, 700, 500);
}

void handlingIncoming(const char *in) {
  if (strcmp(in, "ACK") == 0) {}
  else if (strcmp(in, "COMING") == 0) {}
  else {
    Serial.print(F("BT in: "));
    Serial.println(in);
  }
}

// ---------------- MAIN LOOP ----------------
void loop() {
  // 1) handle incoming BT
  if (BT.available()) {
    char inBuf[32];
    int len = BT.readBytesUntil('\n', inBuf, sizeof(inBuf) - 1);
    inBuf[len] = 0;
    if (len > 0) handlingIncoming(inBuf);
  }

  // 2) Serial commands (SET per-finger)
  if (Serial.available()) {
    char cmd[32];
    int len = Serial.readBytesUntil('\n', cmd, sizeof(cmd) - 1);
    cmd[len] = 0;

    // Per-finger threshold command: SET0 0.7
    if (strncmp(cmd, "SET", 3) == 0 && isdigit(cmd[3])) {
      int finger = cmd[3] - '0';
      float v = atof(cmd + 5);
      if (finger >= 0 && finger < 5 && v >= 0.3 && v <= 0.9) {
        BEND_THRESHOLD[finger] = v;
        Serial.print(F("Finger "));
        Serial.print(finger);
        Serial.print(F(" threshold -> "));
        Serial.println(BEND_THRESHOLD[finger]);
      } else Serial.println(F("Invalid finger or threshold"));
    }
  }

  // 3) SOS button
  if (digitalRead(sosPin) == LOW) {
    sendMessage("SOS|🚨 Emergency! Please help me!");
    playSOSPattern();
    delay(1200);
    return;
  }

  // 4) MPU6050
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  roll  = atan2((float)ay, (float)az) * 180.0 / PI;
  pitch = atan2(-(float)ax, sqrt((float)ay*ay + (float)az*az)) * 180.0 / PI;

  // 5) Detect gesture
  char gesture[24];
  detectGesture(gesture);
  unsigned long now = millis();
  if (gesture[0] && (now - lastSendTime > SEND_COOLDOWN)) {
    char msg[32];
    sprintf(msg, "G|%s", gesture);
    sendMessage(msg);
    lastSendTime = now;
  }

  bufferIndex = (bufferIndex + 1) % SMOOTH_N;
  delay(40);
}
