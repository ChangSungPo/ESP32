//Filename: Lab3Par1.ino
//Author: Sung-Po Chang
//Date: 05/16/2026
//Description: Preemptive Scheduling and Intro to FreeRTOS
//Version: 1.0

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // Initialize the LCD

#define BIT_RS        0x01  // Bit 0: command (0) or data (1)
#define BIT_RW        0x02  // Bit 1: writing to (0) or reading from (1) the LCD
#define BIT_EN        0x04  // Bit 2: HIGH to signal the LCD that data or a command is incoming, and back to LOW to tell the LCD device to process it
#define BIT_BACKLIGHT 0x08  // Bit 3: backlight control bit, turn on (1), and turn off (0)

//Name: lcd_send
//Description: Take value and preprocess it then send to LCD
void lcd_send(uint8_t value, bool is_data) {
  uint8_t rs_bit = is_data ? BIT_RS : 0x00;
  uint8_t backlight_bit = BIT_BACKLIGHT;

  // 1. first half data (Bits 4-7)
  uint8_t high_nibble = value & 0xF0;
  
  Wire.beginTransmission(0x27);
  Wire.write(high_nibble | backlight_bit | BIT_EN | rs_bit);
  Wire.write(high_nibble | backlight_bit | 0x00 | rs_bit);
  Wire.endTransmission();
  delay(1);

  // 2. second half data
  uint8_t low_nibble = (value << 4) & 0xF0;
  
  Wire.beginTransmission(0x27);
  Wire.write(low_nibble | backlight_bit | BIT_EN | rs_bit);
  Wire.write(low_nibble | backlight_bit | 0x00 | rs_bit);
  Wire.endTransmission();
  delay(1);
}

// lcd command with select bit = 0
void lcd_command(uint8_t cmd) {
  lcd_send(cmd, false);
}

// lcd data with select bit = 1
void lcd_data(uint8_t data) {
  lcd_send(data, true);
}


void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  delay(2);
}

String inputBuffer = "";

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    // Enter
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        
        lcd_command(0x01); //clears the LCD screen
        delay(2); //cleaning delay
        
        lcd_command(0x80); //set cursor to the first line

        //read the input string into string buffer
        for (int i = 0; i < inputBuffer.length(); i++) {
          lcd_data(inputBuffer[i]);
        }

        //clear string buffer
        inputBuffer = "";
      }
    } 
    else {
      inputBuffer += c;
    }
  }
}
