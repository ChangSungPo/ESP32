//Filename: Lab2Par3.ino
//Author: Sung-Po Chang
//Date: 04/25/2026
//Description: Controlling Peripherals with a Photoresistor
//Version: 1.0

#define GPIO_LED 1
#define GPIO_SENSOR 7

// LEDC PWM 
const int pwmChannel = 0;    
const int freq = 5000;       // 5000 Hz 
const int resolution = 12;   // 12-bit ADC

void setup() {
  // 1. LEDC
  // ledcSetup(pwmChannel, freq, resolution);  //Version < 3.0

  // 2. LED pin to PWM channel
  // ledcAttachPin(GPIO_LED, pwmChannel);  //Version < 3.0

  ledcAttach(GPIO_LED, freq, resolution);   //Version > 3.0
}

void loop() {
  // 3. read 12-bit ADC, 0 to 4095
  int lightRaw = analogRead(GPIO_SENSOR);

  // 4. 
  // brighter light -> higher voltage -> higher lightRaw
  // When env brighter, LED is darker, vice versa
  // Use 4095 - lightRaw to implement
  int brightness = 4095 - lightRaw;
  if (brightness < 0) brightness = 0;

  // 5. output PWM
  ledcWrite(GPIO_LED, brightness);
}