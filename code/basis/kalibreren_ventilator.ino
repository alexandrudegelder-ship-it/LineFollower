#include <Servo.h>
#define FAN A5

Servo esc;

void setup() {
  esc.attach(FAN, 1000, 2000);  // PWM-range expliciet
  delay(2000);

  // STAP 1 – Max throttle
  esc.writeMicroseconds(1200);
  delay(3000);       // wacht op BEEP BEEP (hoog signaal ontdekt)

  // STAP 2 – Min throttle
  esc.writeMicroseconds(1000);
  delay(3000);       // wacht op bevestiging
}

void loop() {
  // daarna moet hij draaien als test
  esc.writeMicroseconds(1500);
}



