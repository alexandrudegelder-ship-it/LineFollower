#include <SPI.h>
#include <RF24.h>
#include <string.h>
#include <stdlib.h>

RF24 radio(7, 8);
const byte ADDR_SM[6] = "SM001";  // Slave→Master
const byte ADDR_MS[6] = "MS001";  // Master→Slave

const int LED_PIN = 6;

// === Knoppen ===
const int PIN_START = 2;   // INT0
const int PIN_STOP  = 3;   // INT1
const int PIN_RESET = 4;   // PCINT (pin change)
const unsigned long DEBOUNCE_MS = 40;

// LED modes
#define LED_OFF        0
#define LED_STEADY     1
#define LED_BLINK_1HZ  2
#define LED_BLINK_20HZ 3

int ledMode = LED_OFF;
unsigned long lastToggle = 0;
const unsigned long T1  = 500;  // 1 Hz
const unsigned long T20 = 25;   // 20 Hz

// S4 tijd-gestuurd (waterdicht)
bool s4Active = false;
unsigned long s4Start = 0;

// === knop-ISR data (volatile) ===
volatile bool isrStartRaw = false;
volatile bool isrStopRaw  = false;
volatile bool isrResetRaw = false;
volatile bool isrStartDirty = false;
volatile bool isrStopDirty  = false;
volatile bool isrResetDirty = false;

// === debounce/stable ===
bool startStable = false, startLast = false;
bool stopStable  = false, stopLast  = false;
bool resetStable = false, resetLast = false;
unsigned long tStartChange = 0, tStopChange = 0, tResetChange = 0;

void setLed(bool on){ digitalWrite(LED_PIN, on ? HIGH : LOW); }
void setLedMode(int m){
  if (m == ledMode) return;
  ledMode = m;
  lastToggle = millis();
  if (m == LED_OFF)    setLed(false);
  if (m == LED_STEADY) setLed(true);
}

void updateLed(){
  unsigned long now = millis();
  if (ledMode == LED_BLINK_1HZ  && (now - lastToggle >= T1 )) { lastToggle = now; digitalWrite(LED_PIN, !digitalRead(LED_PIN)); }
  if (ledMode == LED_BLINK_20HZ && (now - lastToggle >= T20)) { lastToggle = now; digitalWrite(LED_PIN, !digitalRead(LED_PIN)); }

  // S4: 0–3s knipper, 3–6s steady, >6s uit
  if (s4Active){
    unsigned long dt = now - s4Start;
    if (dt < 3000)      setLedMode(LED_BLINK_1HZ);
    else if (dt < 6000) setLedMode(LED_STEADY);
    else { setLedMode(LED_OFF); s4Active = false; }
  }
}

void startS4IfNeeded(){
  if (!s4Active){ s4Active = true; s4Start = millis(); setLedMode(LED_BLINK_1HZ); }
}
void stopS4(){ s4Active = false; }

// === radio TX helper (master → slave) ===
void sendToSlave(const char* tag){
  radio.stopListening();                    // kort naar TX
  radio.openWritingPipe(ADDR_MS);
  char msg[32] = {0};
  strncpy(msg, tag, sizeof(msg)-1);
  radio.write(msg, strlen(msg)+1);
  radio.startListening();                   // zo snel mogelijk terug naar RX
}

// === ISR's (active LOW) ===
void isrStart(){
  bool raw = (digitalRead(PIN_START) == LOW);
  isrStartRaw = raw; isrStartDirty = true;
}
void isrStop(){
  bool raw = (digitalRead(PIN_STOP) == LOW);
  isrStopRaw = raw; isrStopDirty = true;
}
// D4 via pin-change interrupt (PCINT2_vect)
ISR(PCINT2_vect){
  bool raw = (PIND & _BV(PD4)) == 0;   // LOW = ingedrukt
  isrResetRaw = raw; isrResetDirty = true;
}

void serviceButtons(){
  unsigned long now = millis();

  // START (D2)
  if (isrStartDirty){ isrStartDirty = false; tStartChange = now; }
  if (now - tStartChange >= DEBOUNCE_MS){
    startStable = isrStartRaw;
    bool pressed  = (!startLast && startStable);
    bool released = ( startLast && !startStable);
    if (pressed)  sendToSlave("start");
    if (released) sendToSlave("running");
    startLast = startStable;
  }

  // STOP (D3)
  if (isrStopDirty){ isrStopDirty = false; tStopChange = now; }
  if (now - tStopChange >= DEBOUNCE_MS){
    stopStable = isrStopRaw;
    bool pressed  = (!stopLast && stopStable);
    bool released = ( stopLast && !stopStable);
    if (pressed)  sendToSlave("stop");
    if (released) sendToSlave("ended");
    stopLast = stopStable;
  }

  // RESET (D4)
  if (isrResetDirty){ isrResetDirty = false; tResetChange = now; }
  if (now - tResetChange >= DEBOUNCE_MS){
    resetStable = isrResetRaw;
    bool pressed  = (!resetLast && resetStable);
    bool released = ( resetLast && !resetStable);
    if (pressed)  sendToSlave("reset");
    if (released) sendToSlave("master ready");
    resetLast = resetStable;
  }
}

// === RX (slave → master, bestaand) ===
void handleMessage(const char* msg){
  // --- S4: tijd-gestuurd, berichten enkel trigger ---
  if (!strncmp(msg, "S4|standby", 10)){ startS4IfNeeded(); return; }
  if (!strncmp(msg, "S4|kalibreren compleet", 23)){ startS4IfNeeded(); return; }

  // --- S0 reset ---
  if (!strncmp(msg, "S0|ready", 8)){ setLedMode(LED_OFF); stopS4(); return; }

  // --- S1 (wit) ---
  if (!strncmp(msg, "S1|begin", 8))                { setLedMode(LED_STEADY); stopS4(); return; }
  if (!strncmp(msg, "S1|ready", 8))                { setLedMode(LED_STEADY); return; }
  if (!strncmp(msg, "S1|standby", 10))             { setLedMode(LED_BLINK_1HZ); return; }
  if (!strncmp(msg, "S1|wit kalibreren", 17))      { setLedMode(LED_BLINK_20HZ); return; }
  if (!strncmp(msg, "S1|wit compleet", 15))        { setLedMode(LED_BLINK_1HZ); return; }

  // --- S2 ---
  if (!strncmp(msg, "S2|ready", 8))                { setLedMode(LED_BLINK_1HZ); return; }

  // --- S3 (zwart) ---
  if (!strncmp(msg, "S3|zwart kalibreren", 19))    { setLedMode(LED_BLINK_20HZ); return; }
  if (!strncmp(msg, "S3|standby", 10))             { setLedMode(LED_BLINK_1HZ); return; }
}

void setup(){
  Serial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  // knoppen (naar GND)
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_STOP,  INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP);

  // init raw-states
  isrStartRaw = (digitalRead(PIN_START) == LOW);
  isrStopRaw  = (digitalRead(PIN_STOP)  == LOW);
  isrResetRaw = (digitalRead(PIN_RESET) == LOW);
  startLast = startStable = isrStartRaw;
  stopLast  = stopStable  = isrStopRaw;
  resetLast = resetStable = isrResetRaw;

  // externe interrupts
  attachInterrupt(digitalPinToInterrupt(PIN_START), isrStart, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_STOP),  isrStop,  CHANGE);

  // pin-change interrupt voor D4 (PD4 = PCINT20) → PCINT2 vector
  PCICR  |= _BV(PCIE2);     // enable PCINT[23:16]
  PCMSK2 |= _BV(PCINT20);   // enable PD4 (D4)

  // radio
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.setAutoAck(true);
  radio.setRetries(3, 5);          // korter, minder TX-blokkade
  radio.setPALevel(RF24_PA_LOW);

  radio.openReadingPipe(0, ADDR_SM); // RX: SLAVE -> MASTER
  radio.openWritingPipe(ADDR_MS);    // TX: MASTER -> SLAVE
  radio.startListening();

  Serial.println("MASTER ready");
}

void loop(){
  // verwerk knoppen
  serviceButtons();

  // RX
  if (radio.available()){
    char msg[32] = {0};
    radio.read(msg, sizeof(msg));
    handleMessage(msg);
  }

  // LED
  updateLed();
}





