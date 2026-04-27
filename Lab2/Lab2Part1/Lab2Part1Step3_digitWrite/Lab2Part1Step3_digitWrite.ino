//Filename: Lab2Par1Step3_digitWrite.ino
//Author: Sung-Po Chang
//Date: 04/25/2026
//Description: Compare time between digitWrite function and manipulating hardware registers
//Version: 1.0

const int ledPin = 5;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
}

void loop() {
  unsigned long startTime = micros();

  for (int i = 0; i < 1000; i++) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(ledPin, LOW);
  }

  unsigned long endTime = micros();
  
  Serial.print("DigitalWrite (1000): ");
  Serial.print(endTime - startTime);
  Serial.println(" us");

  delay(1000);
}