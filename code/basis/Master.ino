#include <SPI.h>
#include <RF24.h>

// ---------------- RF24 ----------------
RF24 radio(7, 8);                  // CE, CSN
const byte ADDR_SM[6] = "SM001";   // Slave → Master
const byte ADDR_MS[6] = "MS001";   // Master → Slave

// ---------------- PINS (MASTER) ----------------
// Buttons (INPUT_PULLUP)
#define PIN_BTN_START      2   // D2
#define PIN_BTN_STOP       3   // D3
#define PIN_BTN_RESET      4   // D4
#define PIN_BTN_FAN        5   // D5

// LEDs (OUTPUT)
#define PIN_LED_START      A0
#define PIN_LED_STOP       A1
#define PIN_LED_RESET      A2
#define PIN_LED_ERROR      A3  // blijft uit (geen errorsysteem)
#define PIN_LED_KALIB      A4  // (nog niet gebruikt)

// ---------------- Data ----------------
uint8_t sensorBits[16] = {0};
int potValue = -1;

// ---------------- Debounce ----------------
struct Btn {
  uint8_t pin;
  bool lastRead;
  bool stable;
  unsigned long lastChange;
};

const unsigned long DEB_MS = 40;

Btn bStart { PIN_BTN_START, HIGH, HIGH, 0 };
Btn bStop  { PIN_BTN_STOP,  HIGH, HIGH, 0 };
Btn bReset { PIN_BTN_RESET, HIGH, HIGH, 0 };
Btn bFan   { PIN_BTN_FAN,   HIGH, HIGH, 0 };

static inline bool buttonPressedEdge(Btn &b) {
  bool r = digitalRead(b.pin);

  if (r != b.lastRead) {
    b.lastRead = r;
    b.lastChange = millis();
  }
  if (millis() - b.lastChange < DEB_MS) return false;

  if (r != b.stable) {
    b.stable = r;
    if (b.stable == LOW) return true;  // edge: ingedrukt
  }
  return false;
}

static inline bool buttonIsPressed(const Btn &b) {
  return (digitalRead(b.pin) == LOW);
}

// ---------------- RF send ----------------
static inline void sendText(const char* msg) {
  radio.stopListening();
  radio.write(msg, strlen(msg) + 1);
  radio.startListening();
}

// ---------------- Helpers ----------------
static inline void setStartStopLeds(bool startOn, bool stopOn) {
  digitalWrite(PIN_LED_START, startOn ? HIGH : LOW);
  digitalWrite(PIN_LED_STOP,  stopOn  ? HIGH : LOW);
}

// -------------------------------------------------
void setup() {
  Serial.begin(9600);

  // Buttons
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_STOP,  INPUT_PULLUP);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);
  pinMode(PIN_BTN_FAN,   INPUT_PULLUP);

  // LEDs
  pinMode(PIN_LED_START, OUTPUT);
  pinMode(PIN_LED_STOP,  OUTPUT);
  pinMode(PIN_LED_RESET, OUTPUT);
  pinMode(PIN_LED_ERROR, OUTPUT);
  pinMode(PIN_LED_KALIB, OUTPUT);

  // Default LEDs
  setStartStopLeds(false, false);
  digitalWrite(PIN_LED_RESET, LOW);
  digitalWrite(PIN_LED_ERROR, LOW);
  digitalWrite(PIN_LED_KALIB, LOW);

  // RF init
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.setPALevel(RF24_PA_LOW);

  radio.openReadingPipe(0, ADDR_SM);   // slave → master
  radio.openWritingPipe(ADDR_MS);      // master → slave
  radio.startListening();

  Serial.println("MASTER ready");
}

// -------------------------------------------------
void loop() {
  // Reset LED volgt knop
  digitalWrite(PIN_LED_RESET, buttonIsPressed(bReset) ? HIGH : LOW);

  // ----- RX verwerken -----
  if (radio.available()) {
    char msg[32] = {0};
    radio.read(msg, sizeof(msg));

    // BITSTRING: B0000000000000000
    if (msg[0] == 'B' && strlen(msg) >= 17) {
      for (int i = 0; i < 16; i++) sensorBits[i] = (msg[1 + i] == '1') ? 1 : 0;

      Serial.print("BITS: ");
      for (int i = 0; i < 16; i++) Serial.print(sensorBits[i]);
      Serial.println();
    }
    // POT: P123
    else if (msg[0] == 'P') {
      potValue = atoi(msg + 1);
      Serial.print("POT: ");
      Serial.println(potValue);
    }
    // STATUS
    else {
      Serial.print("STATUS: ");
      Serial.println(msg);
    }
  }

  // ----- Knop events (debounced edges) -----
  bool startEdge = buttonPressedEdge(bStart);
  bool stopEdge  = buttonPressedEdge(bStop);
  bool fanEdge   = buttonPressedEdge(bFan);

  // ----- START -----
  if (startEdge) {
    setStartStopLeds(true, false);

    sendText("StartEngines");
    sendText("VentilatorOn");

    Serial.println("TX: StartEngines");
    Serial.println("TX: VentilatorOn");
  }

  // ----- STOP -----
  if (stopEdge) {
    setStartStopLeds(false, true);

    sendText("StopEngines");
    sendText("VentilatorOff");

    Serial.println("TX: StopEngines");
    Serial.println("TX: VentilatorOff");
  }

  // ----- FAN BUTTON = FAN OFF -----
  if (fanEdge) {
    sendText("VentilatorOff");
    Serial.println("TX: VentilatorOff");
  }
}





