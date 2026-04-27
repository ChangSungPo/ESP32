//Filename: Lab1Par2.ino
//Author: Sung-Po Chang
//Date: 04/10/2026
//Description: This file controls the blinking of the LED with a button on ESP32
//Version: 1.0

//Global variables for button and LED pin
const int buttonPin = 20;
const int ledPin = 19;

void setup() {
  // set up LED pin and button pin on board
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
}

void loop() {
  int buttonState = digitalRead(buttonPin); //digitalRead reads the button state

  if (buttonState == LOW) { //button state is low means button is "pressed"
    digitalWrite(ledPin, HIGH); //turn on LED
  } else {
    digitalWrite(ledPin, LOW); //turn off LED
  }
}
