#include <SPI.h>
#include <RF24.h>

RF24 radio(7,8);
const byte ADDR_SM[6] = "SM001";  // Slave → Master
const byte ADDR_MS[6] = "MS001";  // Master → Slave

// LEDs
#define LED_RUN   6
#define LED_START A0
#define LED_STOP  A1
#define LED_RESET A2
#define LED_ERROR A3
#define LED_CALIB A4    // kalibratie-LED (puur voor kalibratie)

// Buttons
#define PIN_START 2
#define PIN_STOP  3
#define PIN_RESET 4
#define PIN_FAN   5

unsigned long DEBOUNCE_MS = 40;

bool errorActive   = false;
bool resetAllowed  = false;

// --- kalibratie ---
bool calibOK       = false;   // pas starten/fan als deze true is
bool calibRunning  = false;

// debounce vars
volatile bool rawStart=false, dirtyStart=false;
volatile bool rawStop=false,  dirtyStop=false;
volatile bool rawReset=false, dirtyReset=false;

bool stableStart=false, lastStart=false;
bool stableStop=false,  lastStop=false;
bool stableReset=false, lastReset=false;

unsigned long tStart=0, tStop=0, tReset=0;

// FAN debounce
bool fanStable=false, fanLast=false;
unsigned long tFan=0;

// -----------------------------------------------------
// RF send
// -----------------------------------------------------
void sendToSlave(const char* msg){
  radio.stopListening();
  radio.write(msg, strlen(msg)+1);
  radio.startListening();
}

// -----------------------------------------------------
// LED control
// -----------------------------------------------------
void allOff(){
  digitalWrite(LED_RUN,LOW);
  digitalWrite(LED_START,LOW);
  digitalWrite(LED_STOP,LOW);
  digitalWrite(LED_RESET,LOW);
  digitalWrite(LED_ERROR,LOW);
  // LED_CALIB laten we met rust: puur voor kalibratie
}

void showErrorState(){
  digitalWrite(LED_ERROR, HIGH);
  digitalWrite(LED_RUN,   LOW);
  digitalWrite(LED_START, LOW);
  digitalWrite(LED_STOP,  LOW);
  digitalWrite(LED_RESET, LOW);
}

// -----------------------------------------------------
// 2s knipperen op kalibratie-LED
// -----------------------------------------------------
void blinkCalib2s() {
  for (int i = 0; i < 4; i++) {   // 4 x 0.5 s = 2 s
    digitalWrite(LED_CALIB, HIGH);
    delay(250);
    digitalWrite(LED_CALIB, LOW);
    delay(250);
  }
}

// -----------------------------------------------------
// Button actions
// -----------------------------------------------------
void handleStart(){
  if (errorActive) return;
  if (!calibOK)    return;   // blokkeren tot kalibratie klaar

  sendToSlave("start engines");

  digitalWrite(LED_RUN,   HIGH);
  digitalWrite(LED_START, HIGH);
  digitalWrite(LED_STOP,  LOW);
}

void handleStop(){
  if (errorActive) return;
  sendToSlave("stop engines");

  digitalWrite(LED_RUN, LOW);
  digitalWrite(LED_STOP, HIGH);
  digitalWrite(LED_START, LOW);
}

void handleReset(){
  if (!errorActive) return;
  if (!resetAllowed) return;

  sendToSlave("resetting");

  digitalWrite(LED_ERROR, LOW);
  digitalWrite(LED_RESET, HIGH);

  delay(150);
  digitalWrite(LED_RESET, LOW);

  errorActive   = false;
  resetAllowed  = false;
}

void handleFan(){
  if (errorActive) return;
  if (!calibOK)    return;   // fan ook pas na kalibratie
  sendToSlave("ventilator on");
}

// -----------------------------------------------------
// Interrupt handlers
// -----------------------------------------------------
void isrStart(){
  rawStart = (digitalRead(PIN_START)==LOW);
  dirtyStart = true;
}
void isrStop(){
  rawStop = (digitalRead(PIN_STOP)==LOW);
  dirtyStop = true;
}
ISR(PCINT2_vect){
  rawReset = ((PIND & _BV(PD4))==0);
  dirtyReset = true;
}

// -----------------------------------------------------
// Debounce + logic
// -----------------------------------------------------
void serviceButtons(){
  unsigned long now = millis();

  // START
  if (dirtyStart){ dirtyStart=false; tStart=now; }
  if (now - tStart >= DEBOUNCE_MS){
    bool prev = stableStart;
    stableStart = rawStart;
    if (!prev && stableStart) handleStart();
    lastStart = stableStart;
  }

  // STOP
  if (dirtyStop){ dirtyStop=false; tStop=now; }
  if (now - tStop >= DEBOUNCE_MS){
    bool prev = stableStop;
    stableStop = rawStop;
    if (!prev && stableStop) handleStop();
    lastStop = stableStop;
  }

  // RESET
  if (dirtyReset){ dirtyReset=false; tReset=now; }
  if (now - tReset >= DEBOUNCE_MS){
    bool prev = stableReset;
    stableReset = rawReset;
    if (!prev && stableReset) handleReset();
    lastReset = stableReset;
  }

  // FAN
  bool rawFan = (digitalRead(PIN_FAN)==LOW);
  if (rawFan != fanLast) { fanLast=rawFan; tFan=now; }
  if ((now - tFan) >= DEBOUNCE_MS) {
    if (fanStable != fanLast){
      bool prev = fanStable;
      fanStable = fanLast;
      if (!prev && fanStable) handleFan();
    }
  }
}

// -----------------------------------------------------
// RF receive
// -----------------------------------------------------
void handleIncoming(const char* msg){

  Serial.print("RX: ");
  Serial.println(msg);

  // ---------- KALIBRATIE ----------
  if (!strncmp(msg,"wit kalibreren",14)) {
    calibRunning = true;
    calibOK      = false;

    // 2 s knipperen
    blinkCalib2s();
    digitalWrite(LED_CALIB, LOW);

    return;
  }

  if (!strncmp(msg,"zwart kalibreren",16)) {
    calibRunning = true;
    calibOK      = false;

    // 2 s knipperen
    blinkCalib2s();
    digitalWrite(LED_CALIB, LOW);

    return;
  }

  if (!strncmp(msg,"kalibreren compleet",19)) {
    calibRunning = false;
    calibOK      = true;

    // 2 s constant aan
    digitalWrite(LED_CALIB, HIGH);
    delay(2000);
    digitalWrite(LED_CALIB, LOW);

    return;
  }

  // ---------- ERROR ----------
  if (!strncmp(msg,"error",5)){
    errorActive  = true;
    resetAllowed = false;
    showErrorState();
    return;
  }

  // ---------- EDGE ----------
  if (!strncmp(msg,"edge:1",6)){
    if (errorActive) resetAllowed = true;
    return;
  }
}

// -----------------------------------------------------
// Setup
// -----------------------------------------------------
void setup(){
  Serial.begin(9600);

  pinMode(LED_RUN,OUTPUT);
  pinMode(LED_START,OUTPUT);
  pinMode(LED_STOP,OUTPUT);
  pinMode(LED_RESET,OUTPUT);
  pinMode(LED_ERROR,OUTPUT);
  pinMode(LED_CALIB,OUTPUT);
  allOff();
  digitalWrite(LED_CALIB, LOW);

  pinMode(PIN_START,INPUT_PULLUP);
  pinMode(PIN_STOP,INPUT_PULLUP);
  pinMode(PIN_RESET,INPUT_PULLUP);
  pinMode(PIN_FAN,INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_START), isrStart, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_STOP),  isrStop,  CHANGE);

  PCICR |= _BV(PCIE2);
  PCMSK2 |= _BV(PCINT20);

  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);

  radio.openReadingPipe(0,ADDR_SM);
  radio.openWritingPipe(ADDR_MS);
  radio.startListening();

  Serial.println("MASTER ready");
  sendToSlave("master ready");
}

// -----------------------------------------------------
// Main loop
// -----------------------------------------------------
void loop(){
  serviceButtons();

  if (radio.available()){
    char msg[32]={0};
    radio.read(msg,sizeof(msg));
    handleIncoming(msg);
  }

  // Failsafe: LEDs altijd in error-modus houden
  if (errorActive){
    showErrorState();
  }
}





