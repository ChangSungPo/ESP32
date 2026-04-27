//Filename: Lab1Par3.ino
//Author: Sung-Po Chang
//Date: 04/10/2026
//Description: This file controls the blinking of two LED with seperate button on ESP32
//Version: 1.0

// ================ Global variables ================
const int led1 = 19;
const int btn1 = 20;
const int led2 = 38;
const int btn2 = 2;
bool flashing1 = true;
bool flashing2 = true;


void setup() {
  // set up LED pins and button pins on board
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(btn1, INPUT_PULLUP);
  pinMode(btn2, INPUT_PULLUP);
}

void loop() {
  checkButton(btn1, flashing1); //check button state for LED1
  checkButton(btn2, flashing2); //check button state for LED2

  if (flashing1) {
    blinkLED(led1, 500);
  } else {
    digitalWrite(led1, LOW);
  }

  if (flashing2) {
    blinkLED(led2, 500);
  } else {
    digitalWrite(led2, LOW);
  }
}

// Name: checkButton
// Description: This function takes button pin number and LED current state to determine to switch LED state or not
void checkButton(int btnPin, bool &state) {
  if (digitalRead(btnPin) == LOW) {
    state = !state;
    delay(200);
  }
}

// Name: blinkLED
// Description: This function takes pin number and delay time to implement the blinking behavior
void blinkLED(int pin, int delayTime) {
  digitalWrite(pin, HIGH);
  delay(delayTime);
  digitalWrite(pin, LOW);
  delay(delayTime);  
}