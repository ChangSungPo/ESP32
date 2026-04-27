//Filename: Lab1Par1.ino
//Author: Sung-Po Chang
//Date: 04/09/2026
//Description: This file controls the blinking of the LED on ESP32
//Version: 1.0

void setup() {
  // set up two LED pins on board
  pinMode(19, OUTPUT);
  pinMode(48, OUTPUT);
}

void loop() {
  // loop blinking function
  blinkLED(19, 100);
  blinkLED(48, 1000);
}

// Name: blinkLED
// Description: This function takes pin number and delay time to implement the blinking behavior
void blinkLED(int pin, int delayTime) {
  digitalWrite(pin, HIGH);
  delay(delayTime);
  digitalWrite(pin, LOW);
  delay(delayTime);  
}
