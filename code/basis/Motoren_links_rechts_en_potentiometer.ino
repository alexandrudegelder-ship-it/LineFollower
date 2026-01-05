#include <SPI.h>
#include <RF24.h>

// ---------------- RF24 ----------------
RF24 radio(7, 8);
const byte ADDR_SM[6] = "SM001";   // Slave → Master
const byte ADDR_MS[6] = "MS001";   // Master → Slave

// ---------------- TB6612 motor driver ----------------
#define AIN1 5
#define AIN2 4
#define PWMA 3
#define BIN1 6
#define BIN2 9
#define PWMB 10

#define POT A7
#define BTN 2   // blijft zo (niet gebruikt)

// -------------------------------------------------
static inline void sendToMasterText(const char* msg) {
  radio.stopListening();
  radio.write(msg, strlen(msg) + 1);
  radio.startListening();
}

// Motor A forward (links)
static inline void motorA_forward(uint8_t pwm) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, pwm);
}

// Motor B forward (rechts)
static inline void motorB_forward(uint8_t pwm) {
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, pwm);
}

static inline void motorA_stop() {
  analogWrite(PWMA, 0);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
}
static inline void motorB_stop() {
  analogWrite(PWMB, 0);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
}
static inline void allStop() { motorA_stop(); motorB_stop(); }

// -------------------------------------------------
// Test: 2s links (A), 1s stop, 2s rechts (B), 1s stop
// Stuurt enkel tekst bij start van A of B fase
// -------------------------------------------------
static inline void motorSideTest(uint8_t pwm) {
  static uint8_t phase = 0;
  static unsigned long t0 = 0;

  const unsigned long T_RUN   = 2000;
  const unsigned long T_PAUSE = 1000;

  unsigned long dur = (phase % 2 == 0) ? T_RUN : T_PAUSE;
  if (millis() - t0 < dur) return;
  t0 = millis();

  switch (phase) {
    case 0:
      motorA_forward(pwm);
      motorB_stop();
      sendToMasterText("AIN1 AIN2");
      break;
    case 1:
      allStop();
      break;
    case 2:
      motorA_stop();
      motorB_forward(pwm);
      sendToMasterText("BIN1 BIN2");
      break;
    case 3:
      allStop();
      break;
  }
  phase = (phase + 1) & 3;
}

// -------------------------------------------------
unsigned long lastPotTX = 0;

void setup() {
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);

  pinMode(POT, INPUT);
  pinMode(BTN, INPUT_PULLUP);

  allStop();

  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.setPALevel(RF24_PA_LOW);

  radio.openReadingPipe(0, ADDR_MS);
  radio.openWritingPipe(ADDR_SM);
  radio.startListening();
}

void loop() {
  // pot 0..255
  uint8_t pwm = (uint8_t)map(analogRead(POT), 0, 1023, 0, 255);

  // motor test blijft lopen
  motorSideTest(pwm);

  // 1x per seconde potwaarde naar master
  if (millis() - lastPotTX >= 1000) {
    lastPotTX = millis();

    char msg[16];
    snprintf(msg, sizeof(msg), "POT %u", pwm);
    sendToMasterText(msg);
  }
}


