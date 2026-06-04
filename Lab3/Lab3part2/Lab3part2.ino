#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define BIT_RS        0x01  // Bit 0: command (0) or data (1)
#define BIT_RW        0x02  // Bit 1: writing to (0) or reading from (1) the LCD
#define BIT_EN        0x04  // Bit 2: HIGH to signal the LCD that data or a command is incoming, and back to LOW to tell the LCD device to process it
#define BIT_BACKLIGHT 0x08  // Bit 3: backlight control bit, turn on (1), and turn off (0)

#define LED_BLINK_PIN  4    // Task 1: LED Blinker
#define LED_MUSIC_PIN  47    // Task 3: LED for buzzer
#define BUZZER_PIN     42    // Task 3: buzzer

#define BUZZER_CHANNEL 0
#define LED_CHANNEL    1

LiquidCrystal_I2C lcd(0x27, 16, 2); 

// TCB (Task Control Block)
struct TCB {
  String name;           // Task name
  int priority;          // priority (1-4)
  bool isDone;           // is this task finish
  void (*task_func)();   // function pointer
};

// Task function
void taskLEDBlinker();
void taskCounter();
void taskMusicPlayer();
void taskAlphabetPrinter();

// TCB array
TCB myTasks[4] = {
  {"LED Blinker",      4, false, taskLEDBlinker},
  {"Counter",          3, false, taskCounter},
  {"Music Player",     2, false, taskMusicPlayer},
  {"Alphabet Printer", 1, false, taskAlphabetPrinter}
};

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

void lcd_print(const char* str) {
  while (*str) {
    lcd_data(*str++);
  }
}

void lcd_print_int(int num) {
  if (num == 10) {
    lcd_data('1');
    lcd_data('0');
  } else {
    lcd_data('0' + num); // 1~9 ASCII
  }
}

// Task 1: LED Blinker (Turn an external LED on and off eight times in one second intervals)
void taskLEDBlinker() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_BLINK_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_BLINK_PIN, LOW);
    delay(1000);
  }
}

// Task 2: Counter (Count up from 1 to 10 on LCD)
void taskCounter() {
  lcd_command(0x01); //clears the LCD screen
  delay(2);          //cleaning delay
  
  for (int i = 1; i <= 10; i++) {
    lcd_command(0x80); //set cursor to the first line
    lcd_print("Count: ");
    lcd_print_int(i);  //print int
    delay(500); 
  }
}

// Task 3: Music player (Play a melody of ten notes on a buzzer and display the “notes” (read: different voltage levels) on a second LED.)
void taskMusicPlayer() {
  // Define 10 music symbol
  int notes[10] = {262, 294, 330, 349, 392, 440, 494, 523, 587, 659};
  // Define 10 different voltage corronsponding 10 music symbol
  int brightness[10] = {25, 50, 75, 100, 125, 150, 175, 200, 225, 255};

  for (int i = 0; i < 10; i++) {
    // ledcSetup(BUZZER_CHANNEL, notes[i], 8);
    // ledcWrite(BUZZER_CHANNEL, 128);
    ledcWriteTone(BUZZER_PIN, notes[i]);
    
    // Second LED voltage
    ledcWrite(LED_MUSIC_PIN, brightness[i]);
    
    delay(400);
  }
  
  ledcWrite(BUZZER_PIN, 0);
  ledcWrite(LED_MUSIC_PIN, 0);
}

// Task 4：Alphabet Printer
void taskAlphabetPrinter() {
  for (char c = 'A'; c <= 'Z'; c++) {
    Serial.print(c);
    if (c < 'Z') Serial.print(", ");
    delay(50); 
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); 
  lcd.init();
  delay(2);

  // GPIO
  pinMode(LED_BLINK_PIN, OUTPUT);
  // ledcSetup(BUZZER_CHANNEL, 2000, 8); 
  // ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  // ledcSetup(LED_CHANNEL, 5000, 8);    
  // ledcAttachPin(LED_MUSIC_PIN, LED_CHANNEL);

  ledcAttach(BUZZER_PIN, 2000, 8);
  ledcAttach(LED_MUSIC_PIN, 5000, 8);

  Serial.println("--- Scheduler Initialized with Manual LCD Driver ---");
}

void loop() {
  int highestPriority = -1;
  int targetIndex = -1;

  for (int i = 0; i < 4; i++) {
    if (!myTasks[i].isDone) {
      if (myTasks[i].priority > highestPriority) {
        highestPriority = myTasks[i].priority;
        targetIndex = i;
      }
    }
  }

  if (targetIndex != -1) {
    myTasks[targetIndex].task_func();
    myTasks[targetIndex].isDone = true;
    
    Serial.print(myTasks[targetIndex].name);
    Serial.print(": ");
    Serial.println(myTasks[targetIndex].priority);
  } 
  // Round finish，update priorities
  else {
    Serial.println("===========================================");
    Serial.println("All tasks completed! Cycling priorities...");
    Serial.println("===========================================");
    
    for (int i = 0; i < 4; i++) {
      myTasks[i].isDone = false;
      myTasks[i].priority = (myTasks[i].priority % 4) + 1;
    }
    
    delay(2000); 
  }
}