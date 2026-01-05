#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <math.h> 

RF24 radio(7, 8);
const byte ADDR_SM[6] = "SM001";
const byte ADDR_MS[6] = "MS001";

#define MUX_S0  A0
#define MUX_S1  A1
#define MUX_S2  A2
#define MUX_S3  A3
#define SIG     A6
#define MOSFET  A4
#define BTN     2 

#define AIN1    4
#define AIN2    5
#define BIN1    6
#define BIN2    9
#define PWMA    3
#define PWMB    10

#define ESC_PIN A5
const uint16_t ESC_IDLE_US   = 1100;
const uint16_t ESC_RUN_US    = 1400;
const uint16_t ESC_PERIOD_US = 20000;

const unsigned long CYCLE_US = 8000;
static unsigned long lastCycleUs = 0;

int sensorOrder[16] = { 8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7 };
uint16_t sensorVal[16];
uint16_t avgWhite[16];
uint16_t avgBlack[16];
float w[16];
float linePosMM = 9999.0f;
uint8_t dig[16];
uint8_t calibStep = 0;
const uint8_t  N_SAMPLES = 8;
const uint16_t SAMPLE_DELAY_MS = 10;

const float HALF_EFF = 37.4f;  
const float L_EFF    = 74.8f; 

unsigned long lastTX = 0;
const unsigned long TX_MS = 200;

static const uint32_t EEPROM_MAGIC = 0xC0A1BEEF;
struct CalData {
  uint32_t magic;
  uint16_t white[16];
  uint16_t black[16];
};

bool lastBtnRead = HIGH;
bool lastBtnStable = HIGH;
unsigned long lastDeb = 0;
const unsigned long DEB_MS = 40;

bool enginesRunning = false;
bool fanOn = false;
uint8_t motorPwm = 0;

// ---------- ESC CONTROLLER ----------
unsigned long escNextStartUs  = 0;
unsigned long escPulseStartUs = 0;
bool escHigh = false;
uint16_t escPulseUs = ESC_IDLE_US;

static inline void escUpdateNonBlocking() {
  unsigned long now = micros();
  if (!escHigh) {
    if ((long)(now - escNextStartUs) >= 0) {
      escPulseStartUs = now;
      digitalWrite(ESC_PIN, HIGH);
      escHigh = true;
    }
  } else {
    if ((unsigned long)(now - escPulseStartUs) >= escPulseUs) {
      digitalWrite(ESC_PIN, LOW);
      escHigh = false;
      escNextStartUs = escPulseStartUs + ESC_PERIOD_US;
    }
  }
}

// ---------------- COMMUNICATIE ----------------
static inline void sendText(const char* msg) {
  radio.stopListening();
  radio.write(msg, strlen(msg) + 1);
  radio.startListening();
}

static inline void sendConfirm(const char* msg) {
  for (uint8_t i = 0; i < 3; i++) {
    sendText(msg);
    delay(30); 
  }
}

static inline void sendPosToMaster() {
  char posStr[10];
  dtostrf(linePosMM, 6, 2, posStr);
  char msg[16];
  snprintf(msg, sizeof(msg), "M%s", posStr);
  sendText(msg);
}

// ---------------- MOTOREN ----------------
static inline void setMotorSigned(int left, int right) {
  if (left >= 0) { digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH); }
  else { digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); left = -left; }
  if (right >= 0) { digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); }
  else { digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH); right = -right; }
  analogWrite(PWMA, constrain(left, 0, 255));
  analogWrite(PWMB, constrain(right, 0, 255));
}

static inline void enginesStart() {
  enginesRunning = true;
  sendConfirm("EnginesRunning");
}

static inline void enginesStop() {
  enginesRunning = false;
  setMotorSigned(0, 0);
  sendConfirm("EnginesStopped");
}

// ---------------- SENSORIEK ----------------
static inline void updateSensors() {
  for (int i = 0; i < 16; i++) {
    if (i <= 1 || i >= 14) { sensorVal[i] = 0; continue; }
    int ch = sensorOrder[i];
    digitalWrite(MUX_S0, ch & 1); digitalWrite(MUX_S1, ch & 2);
    digitalWrite(MUX_S2, ch & 4); digitalWrite(MUX_S3, ch & 8);
    sensorVal[i] = (uint16_t)analogRead(SIG);
  }
}

static inline void captureAverage(uint16_t outAvg[16]) {
  uint32_t acc[16] = {0};
  for (uint8_t k = 0; k < N_SAMPLES; k++) {
    updateSensors();
    for (int i = 0; i < 16; i++) acc[i] += sensorVal[i];
    delay(SAMPLE_DELAY_MS);
  }
  for (int i = 0; i < 16; i++) outAvg[i] = (uint16_t)(acc[i] / N_SAMPLES);
}

static inline void computeAll() {
  if (calibStep != 2) {
    for (int i = 0; i < 16; i++) { w[i] = 0.0f; dig[i] = 0; }
    linePosMM = 9999.0f;
    return;
  }
  updateSensors();

  int activeCount = 0;
  for (int i = 2; i <= 13; i++) {
    float white = (float)avgWhite[i]; float black = (float)avgBlack[i];
    float v = (float)sensorVal[i]; float denom = black - white;
    if (denom < 5.0f) { w[i] = 0.0f; dig[i] = 0; continue; }
    w[i] = constrain((v - white) / denom, 0.0f, 1.0f);
    float thr = (white + black) * 0.5f;
    dig[i] = (v > thr) ? 1 : 0;
    if (dig[i]) activeCount++;
  }

  float sumW = 0.0f; float sumX = 0.0f;
  for (int i = 2; i <= 13; i++) {
    if (dig[i]) {
      bool hasNeighbor = false;
      if (i > 2 && dig[i-1]) hasNeighbor = true;
      if (i < 13 && dig[i+1]) hasNeighbor = true;
      if (hasNeighbor || activeCount == 1) { 
        float x = -HALF_EFF + (float)(i - 2) * (L_EFF / 11.0f);
        sumW += w[i];
        sumX += w[i] * x;
      } else {
        dig[i] = 0; 
      }
    }
  }
  if (sumW < 0.02f) linePosMM = 9999.0f;
  else linePosMM = sumX / sumW;
}

// ---------------- STUURLOGICA (AGRESSIEVE BOCHT VERSIE) ----------------
static float lastPos = 0;
static float lastKnownPos = 0;

static inline void applyLineFollowPID() {
  computeAll();
  int base = (int)motorPwm;

  if (linePosMM > 9000.0f) { 
      if (lastKnownPos > 0) setMotorSigned(base + 85, -145); 
      else                  setMotorSigned(-145, base + 85);
      return; 
  }

  float pos = linePosMM;
  float absPos = fabs(pos);
  float diff = pos - lastPos; 
  
  if (absPos < 3.0f) {
      setMotorSigned(base, base); 
      lastPos = pos;
      return;
  }

  // Basis alertheid
  float pFactor = 0.75f + (absPos * 0.015f); 
  
  float dFactor;
  if ((pos > 0 && diff < 0) || (pos < 0 && diff > 0)) {
      dFactor = 12.0f; // Sneller terugkeren naar de lijn
  } else {
      dFactor = 6.0f;  
  }

  float pTerm = pos * pFactor;
  float dTerm = diff * dFactor;
  lastPos = pos;
  lastKnownPos = pos;

  int output = (int)(pTerm + dTerm);

  // --- BOCHT HULP: Grijpt eerder en harder in ---
  int finalOutput = output;
  if (absPos > 8.0f) {
      float extra = (absPos - 8.0f) * 4.5f; 
      finalOutput += (pos > 0) ? (int)extra : -(int)extra;
  }

  int finalSteer = constrain(finalOutput, -250, 250);
  setMotorSigned(base + finalSteer, base - finalSteer);
}

// ---------------- EEPROM, SETUP & LOOP ----------------
static inline void saveCalToEEPROM() {
  CalData d; d.magic = EEPROM_MAGIC;
  for (int i = 0; i < 16; i++) { d.white[i] = avgWhite[i]; d.black[i] = avgBlack[i]; }
  EEPROM.put(0, d);
}

static inline bool loadCalFromEEPROM() {
  CalData d; EEPROM.get(0, d);
  if (d.magic != EEPROM_MAGIC) return false;
  for (int i = 0; i < 16; i++) { avgWhite[i] = d.white[i]; avgBlack[i] = d.black[i]; }
  return true;
}

void setup() {
  Serial.begin(9600);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(MOSFET, OUTPUT); digitalWrite(MOSFET, HIGH);
  pinMode(MUX_S0, OUTPUT); pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT); pinMode(MUX_S3, OUTPUT);
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(PWMA, OUTPUT); pinMode(PWMB, OUTPUT);
  pinMode(ESC_PIN, OUTPUT); digitalWrite(ESC_PIN, LOW);
  escNextStartUs = micros();

  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.setPALevel(RF24_PA_LOW);
  radio.openReadingPipe(0, ADDR_MS);
  radio.openWritingPipe(ADDR_SM);
  radio.startListening();

  if (loadCalFromEEPROM()) calibStep = 2;
  lastCycleUs = micros();
}

void loop() {
  escUpdateNonBlocking();
  while (radio.available()) {
    char msg[32] = {0}; radio.read(msg, sizeof(msg));
    if (strcmp(msg, "StartEngines") == 0) enginesStart();
    else if (strcmp(msg, "StopEngines") == 0) { enginesStop(); fanOn = false; escPulseUs = ESC_IDLE_US; }
    else if (strcmp(msg, "VentilatorOn") == 0) { fanOn = true; escPulseUs = ESC_RUN_US; }
    else if (strcmp(msg, "VentilatorOff") == 0) { fanOn = false; escPulseUs = ESC_IDLE_US; }
  }

  bool r = digitalRead(BTN);
  if (r != lastBtnRead) { lastBtnRead = r; lastDeb = millis(); }
  if (millis() - lastDeb >= DEB_MS) {
    if (r != lastBtnStable) {
      lastBtnStable = r;
      if (lastBtnStable == LOW) {
        if (calibStep == 2) { calibStep = 0; sendConfirm("herkalibratie start"); }
        else if (calibStep == 0) { captureAverage(avgWhite); calibStep = 1; }
        else if (calibStep == 1) { captureAverage(avgBlack); saveCalToEEPROM(); calibStep = 2; }
      }
    }
  }

  unsigned long nowUs = micros();
  if (nowUs - lastCycleUs >= CYCLE_US) {
    lastCycleUs += CYCLE_US;
    motorPwm = (uint8_t)(analogRead(A7) / 4);
    if (enginesRunning) applyLineFollowPID();
  }

  if (calibStep == 2 && millis() - lastTX >= TX_MS) {
    lastTX = millis();
    computeAll();
    char bMsg[18]; bMsg[0] = 'B';
    for (int i = 0; i < 16; i++) bMsg[1 + i] = dig[i] ? '1' : '0';
    bMsg[17] = '\0';
    sendText(bMsg);
    char pMsg[16]; snprintf(pMsg, sizeof(pMsg), "P%d", (int)(motorPwm));
    sendText(pMsg);
    sendPosToMaster();
  }
}