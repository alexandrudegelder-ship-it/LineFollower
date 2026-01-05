#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>

// ---------------- RF24 ----------------
RF24 radio(7, 8);
const byte ADDR_SM[6] = "SM001";   // Slave → Master
const byte ADDR_MS[6] = "MS001";   // Master → Slave

// ------------- Sensor / MUX -------------
#define MUX_S0  A0
#define MUX_S1  A1
#define MUX_S2  A2
#define MUX_S3  A3
#define SIG     A6
#define MOSFET  A4
#define BTN     2    // INPUT_PULLUP

// volgorde links → rechts (logische index 0..15)
int sensorOrder[16] = { 8,9,10,11,12,13,14,15, 0,1,2,3,4,5,6,7 };

// actuele meting
uint16_t sensorVal[16];

// kalibratie gemiddelden
uint16_t avgWhite[16];
uint16_t avgBlack[16];

// intern (voor later PID)
float w[16];
float linePosMM = 9999.0f;

// naar master (debug)
uint8_t dig[16];   // 0/1 per sensor

// 0=wit, 1=zwart, 2=idle (kalibratie beschikbaar)
uint8_t calibStep = 0;

const uint8_t  N_SAMPLES = 8;
const uint16_t SAMPLE_DELAY_MS = 10;

// geometrie (uitleg: totale boog 122 mm, maar sensoren 0 en 15 zijn uitgezet)
const float L_TOTAL  = 122.0f;
const float DX_TOTAL = L_TOTAL / 15.0f;
const float L_EFF    = 13.0f * DX_TOTAL;
const float HALF_EFF = L_EFF / 2.0f;

// 5x/sec
unsigned long lastTX = 0;
const unsigned long TX_MS = 200;

// ---------------- EEPROM ----------------
static const uint32_t EEPROM_MAGIC = 0xC0A1BEEF;
static const int EEPROM_ADDR = 0;

struct CalData {
  uint32_t magic;
  uint16_t white[16];
  uint16_t black[16];
};

// ---------------- BTN debounce ----------------
bool lastBtnRead   = HIGH;
bool lastBtnStable = HIGH;
unsigned long lastDeb = 0;
const unsigned long DEB_MS = 40;

// -------------------------------------------------
static inline void sendText(const char* msg) {
  radio.stopListening();
  radio.write(msg, strlen(msg) + 1);
  radio.startListening();
}

static inline void sendConfirm(const char* msg) {
  // 3x sturen zodat master het zeker ziet
  for (uint8_t i = 0; i < 3; i++) {
    sendText(msg);
    delay(30);
  }
}

static inline float clamp01(float x) {
  if (x < 0.0f) return 0.0f;
  if (x > 1.0f) return 1.0f;
  return x;
}

// -------------------------------------------------
static inline void updateSensors() {
  for (int i = 0; i < 16; i++) {
    // uitersten niet gebruiken -> 0 houden
    if (i == 0 || i == 15) {
      sensorVal[i] = 0;
      continue;
    }

    int ch = sensorOrder[i];
    digitalWrite(MUX_S0, ch & 1);
    digitalWrite(MUX_S1, ch & 2);
    digitalWrite(MUX_S2, ch & 4);
    digitalWrite(MUX_S3, ch & 8);

    sensorVal[i] = (uint16_t)analogRead(SIG);
  }
}

// -------------------------------------------------
static inline void captureAverage(uint16_t outAvg[16]) {
  uint32_t acc[16] = {0};

  for (uint8_t k = 0; k < N_SAMPLES; k++) {
    updateSensors();
    for (int i = 0; i < 16; i++) acc[i] += sensorVal[i];
    delay(SAMPLE_DELAY_MS);
  }

  for (int i = 0; i < 16; i++) outAvg[i] = (uint16_t)(acc[i] / N_SAMPLES);

  outAvg[0] = 0;
  outAvg[15] = 0;
}

// -------------------------------------------------
static inline void saveCalToEEPROM() {
  CalData d;
  d.magic = EEPROM_MAGIC;
  for (int i = 0; i < 16; i++) {
    d.white[i] = avgWhite[i];
    d.black[i] = avgBlack[i];
  }
  EEPROM.put(EEPROM_ADDR, d);
}

static inline bool loadCalFromEEPROM() {
  CalData d;
  EEPROM.get(EEPROM_ADDR, d);
  if (d.magic != EEPROM_MAGIC) return false;

  for (int i = 0; i < 16; i++) {
    avgWhite[i] = d.white[i];
    avgBlack[i] = d.black[i];
  }
  return true;
}

static inline bool buttonPressed() {
  bool r = digitalRead(BTN);
  if (r != lastBtnRead) { lastBtnRead = r; lastDeb = millis(); }
  if (millis() - lastDeb < DEB_MS) return false;

  if (r != lastBtnStable) {
    lastBtnStable = r;
    if (lastBtnStable == LOW) return true;
  }
  return false;
}

// -------------------------------------------------
// Intern: gewicht + centroid
// Extern: dig[] (0/1) via threshold = (white+black)/2
// -------------------------------------------------
static inline void computeAll() {
  if (calibStep != 2) {
    for (int i = 0; i < 16; i++) { w[i] = 0.0f; dig[i] = 0; }
    linePosMM = 9999.0f;
    return;
  }

  updateSensors();

  float sumW = 0.0f;
  float sumX = 0.0f;

  dig[0] = 0; dig[15] = 0;
  w[0] = 0.0f; w[15] = 0.0f;

  for (int i = 1; i <= 14; i++) {
    float white = (float)avgWhite[i];
    float black = (float)avgBlack[i];
    float v     = (float)sensorVal[i];

    float denom = black - white;
    if (denom < 5.0f) {
      w[i] = 0.0f;
      dig[i] = 0;
      continue;
    }

    // gewicht 0..1
    float wi = clamp01((v - white) / denom);
    w[i] = wi;

    // 0/1 (lijn=1) op midden-threshold
    float thr = (white + black) * 0.5f;
    dig[i] = (v > thr) ? 1 : 0;  // bij jou: zwart is hoog

    // x in mm (effectieve breedte zonder uitersten)
    float x = -HALF_EFF + (float)(i - 1) * (L_EFF / 13.0f);
    sumW += wi;
    sumX += wi * x;
  }

  if (sumW < 0.02f) linePosMM = 9999.0f;
  else linePosMM = sumX / sumW;
}

// -------------------------------------------------
// Stuur 0/1 array naar master als 1 compacte string
// voorbeeld: B0100011100000000
// -------------------------------------------------
static inline void sendBitsToMaster() {
  char msg[18]; // 'B' + 16 bits + '\0'
  msg[0] = 'B';
  for (int i = 0; i < 16; i++) {
    msg[1 + i] = dig[i] ? '1' : '0';
  }
  msg[17] = '\0';
  sendText(msg);
}

// -------------------------------------------------
// Stuur potentiometerwaarde naar master (1x per sec)
// -------------------------------------------------
static inline void sendPotToMaster() {
  int pot = analogRead(A7) / 4;  // 0..255
  char msg[16];
  snprintf(msg, sizeof(msg), "P%d", pot);
  sendText(msg);
}

// -------------------------------------------------
static inline void handleCalibration() {
  if (!buttonPressed()) return;

  // als kalibratie al beschikbaar: herkalibratie starten
  if (calibStep == 2) {
    calibStep = 0;
    sendConfirm("herkalibratie start");
    return;
  }

  if (calibStep == 0) {
    sendConfirm("wit kalibreren");
    captureAverage(avgWhite);
    sendConfirm("wit compleet");
    calibStep = 1;
  } 
  else if (calibStep == 1) {
    sendConfirm("zwart kalibreren");
    captureAverage(avgBlack);
    sendConfirm("zwart compleet");

    saveCalToEEPROM();
    sendConfirm("kalibreren compleet");

    calibStep = 2;
  }
}

// -------------------------------------------------
void setup() {
  pinMode(BTN, INPUT_PULLUP);

  pinMode(MOSFET, OUTPUT);
  digitalWrite(MOSFET, HIGH);

  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S3, OUTPUT);

  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.setPALevel(RF24_PA_LOW);

  radio.openReadingPipe(0, ADDR_MS);
  radio.openWritingPipe(ADDR_SM);
  radio.startListening();

  sendConfirm("slave ready");

  if (loadCalFromEEPROM()) {
    calibStep = 2;
    sendConfirm("kalibratie geladen");
  } else {
    calibStep = 0;
    sendConfirm("kalibratie nodig");
  }
}

// -------------------------------------------------
void loop() {
  handleCalibration();

  // 5x/sec bits sturen (en intern centroid berekenen)
  if (calibStep == 2 && millis() - lastTX >= TX_MS) {
    lastTX = millis();
    computeAll();          // intern: gewicht + centroid
    sendBitsToMaster();    // extern: 16 bits in 1 string
    sendPotToMaster();     // extern: potentiometerwaarde
  }

  // RX leegmaken (optioneel)
  while (radio.available()) {
    char dump[32];
    radio.read(dump, sizeof(dump));
  }
}











