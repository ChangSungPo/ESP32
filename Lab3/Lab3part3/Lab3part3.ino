#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define BUTTON_PIN    7     
#define BIT_RS        0x01  // Bit 0: command (0) or data (1)
#define BIT_RW        0x02  // Bit 1: writing to (0) or reading from (1) the LCD
#define BIT_EN        0x04  // Bit 2: HIGH to signal the LCD that data or a command is incoming, and back to LOW to tell the LCD device to process it
#define BIT_BACKLIGHT 0x08  // Bit 3: backlight control bit, turn on (1), and turn off (0)

LiquidCrystal_I2C lcd(0x27, 16, 2);

//===============> TODO:
// Generate random Service and Characteristic UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID        "2cc2d08b-f544-41b2-806d-499547a4b4f5"
#define CHARACTERISTIC_UUID "7a78bf9f-fc17-4f49-942c-bfec9c095872"

// button and BLE interrupt ending timeing
unsigned long overrideEndTime = 0; 
bool isOverridden = false;

// hardware timer
hw_timer_t *hwTimer = NULL;

// flag for state machine
volatile int  globalCounter = 0;
volatile bool timerFlag     = false;
volatile bool buttonFlag    = false;
volatile bool bleFlag       = false;

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
  char buf[12];
  itoa(num, buf, 10);
  lcd_print(buf);
}


class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // =========> TODO: This callback function will be invoked when signal is
    // 		     received over BLE. Implement the necessary functionality that
    //		     will trigger the message to the LCD.
    bleFlag = true;
  }
};


// ==============> TODO: Write your timer ISR here.
void IRAM_ATTR onTimer() {
  timerFlag = true;
}

// ==============> TODO: Create an ISR function to handle button press here.
void IRAM_ATTR onButtonPress() {
  buttonFlag = true;
}


void setup() {
  Serial.begin(115200);
  // =========> TODO: Initialize LCD display
  Wire.begin(); 
  lcd.init();
  delay(2);

  BLEDevice::init("MyESP32");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                        CHARACTERISTIC_UUID,
                                        BLECharacteristic::PROPERTY_READ |
                                        BLECharacteristic::PROPERTY_WRITE
                                      );


  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
   
   //
   // =========> TODO: create a timer, attach an interrupt, set an alarm which will
   //                  update the counter every second.
   //
  hwTimer = timerBegin(1000000); //1MHz
  timerAttachInterrupt(hwTimer, &onTimer);
  timerAlarm(hwTimer, 1000000, true, 0);   // timer interrupt every  second

  // ========> TODO: Set button pin as input and attach an interrupt
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);
}


void loop() {
 // =========> TODO: Print out an incrementing counter to the LCD.
 //                  If a signal has been received over BLE, print out “New
 //                  Message!” on the LCD.
 //                  If the button has been pressed, print out "Button Pressed"
 //                  on the LCD.
  unsigned long currentMillis = millis();

  // BLE signal
  if (bleFlag) {
    bleFlag = false; 
    isOverridden = true;
    overrideEndTime = currentMillis + 2000; // freeze 2 seconds
       
    lcd_command(0xC0); //set cursor to the second line

    lcd_print("New Message!");
    Serial.println("BLE Event Triggered: New Message!");
  }
  // button press
  else if (buttonFlag) {
    buttonFlag = false; 
    isOverridden = true;
    overrideEndTime = currentMillis + 2000; // freeze 2 seconds
      
    lcd_command(0xC0); //set cursor to the second line

    lcd_print("Button Pressed");
    Serial.println("Hardware Event Triggered: Button Pressed");
  }

  // reset after 2 seconds
  if (isOverridden && (currentMillis >= overrideEndTime)) {
    isOverridden = false;
    lcd_command(0xC0);
    lcd_print("                ");
    lcd_print("Count: ");
    lcd_print_int(globalCounter);
  }

  // updating LCD counting
  if (timerFlag) {
    timerFlag = false; 
    
    if (!isOverridden) {
      globalCounter++;
      lcd_command(0x80);
      lcd_print("Count: ");
      lcd_print_int(globalCounter);
    }
    
    Serial.print("Periodic Timer Tick. Current Count: ");
    Serial.println(globalCounter);
  }

}
