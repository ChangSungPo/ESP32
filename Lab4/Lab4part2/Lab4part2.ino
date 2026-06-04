/**
 * @file Lab4Par2.ino
 * @author Sung-Po Chang
 * @brief Parallel Computing and Synchronization
 * @version 1.0
 * @date 2026-05-25
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define BIT_RS        0x01  // Bit 0: command (0) or data (1)
#define BIT_RW        0x02  // Bit 1: writing to (0) or reading from (1) the LCD
#define BIT_EN        0x04  // Bit 2: HIGH to signal the LCD that data or a command is incoming, and back to LOW to tell the LCD device to process it
#define BIT_BACKLIGHT 0x08  // Bit 3: backlight control bit, turn on (1), and turn off (0)

#define LED_BLINK_PIN  4    // LED Blinker
#define LDR_PIN        5   // LDR Pin

LiquidCrystal_I2C lcd(0x27, 16, 2); 

SemaphoreHandle_t xSemaphore = NULL;
int global_light_level = 0;
int global_sma = 0;
bool data_changed = false;

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

// lcd print
void lcd_print(const char* str) {
  while (*str) {
    lcd_data(*str++);
  }
}

// lcd print int for multi digit
void lcd_print_int(int num) {
  char buf[12];          
  itoa(num, buf, 10);    
  lcd_print(buf);        
}


// Light Detector Task (Core 0) 
// ====================> TODO:
//          1. Initialize Variables
//          2. Loop Continuously
//           - Read light level from the photoresistor.
//           - Take semaphore
//           - Calculate the simple moving average and update variables.
//           - Give semaphore to signal data is ready.
// Task 1: Light Detector Task (Core 0)
void LightDetectorTask(void *pvParameters) {
  int window[5] = {0};
  int head = 0;
  int sum = 0;
  int count = 0;
  
  pinMode(LDR_PIN, INPUT);

  for (;;) {
    // Read voltage from LDR
    int current_val = analogRead(LDR_PIN);
    
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      // calculate SMA, window size if 5
      sum -= window[head];
      window[head] = current_val;
      sum += current_val;
      head = (head + 1) % 5;
      if (count < 5) count++;
      
      int new_sma = sum / count;
      global_light_level = current_val;
      
      // compare old and new value
      if (global_sma != new_sma) {
        global_sma = new_sma;
        data_changed = true;
      }
      
      xSemaphoreGive(xSemaphore);
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}



// LCD Task (Core 0)
// ====================> TODO:
//          1. Initialize Variables
//           2. Loop Continuously
//            - Wait for semaphore.
//            - If data has changed, update the LCD with the new light level and SMA.
//            - Give back the semaphore.
// Task 2: LCD Task (Core 0)
void LCDTask(void *pvParameters) {
  for (;;) {
    // wait signal
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      // update LCD when value is changed
      if (data_changed) {        
        // display light value
        lcd.setCursor(0, 0); 
        lcd.print("RAW VAL: ");
        lcd.print(global_light_level); 
        lcd.print("  "); 

        // display SMA
        lcd.setCursor(0, 1); 
        lcd.print("SMA VAL: ");
        lcd.print(global_sma); 
        lcd.print("  "); 
        
        data_changed = false; // reset flag
      }
      xSemaphoreGive(xSemaphore);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Anomaly Alarm Task (Core 1)
// ====================> TODO:
//            1. Loop Continuously
//             - Wait for semaphore.
//             - Check if SMA indicates a light anomaly (outside thresholds).
//             - If an anomaly is detected, flash a LED signal.
//             - Give back the semaphore.
// Task 3: Anomaly Alarm Task (Core 1)
void AnomalyAlarmTask(void *pvParameters) {
  pinMode(LED_BLINK_PIN, OUTPUT);
  bool trigger_alarm = false;

  for (;;) {
    // wait signal
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      if (global_sma > 3800 || global_sma < 300) {
        trigger_alarm = true;
      } else {
        trigger_alarm = false;
      }
      xSemaphoreGive(xSemaphore);
    }

    // trigger alarm, flash 3 time with 2 sec interval
    if (trigger_alarm) {
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BLINK_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(300)); 
        digitalWrite(LED_BLINK_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(2000)); 
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// Prime Calculation Task (Core 1)
// ====================> TODO:
//            1. Loop from 2 to 5000
//             - Check if the current number is prime.
//             - If prime, print the number to the serial monitor.
// Task 4: Prime Calculation Task (Core 1)
void PrimeCalculationTask(void *pvParameters) {
  for (;;) {
    for (int num = 2; num <= 5000; num++) {
      bool isPrime = true;
      for (int i = 2; i * i <= num; i++) {
        if (num % i == 0) {
          isPrime = false;
          break;
        }
      }
      
      if (isPrime) {
        Serial.print("Prime found: ");
        Serial.println(num);
        
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void setup(){
  // ====================> TODO:
//         1. Initialize pins, serial, LCD, etc
//         2. Create binary semaphore for synchronization of light level data.
//         3. Create Tasks
//          - Create the `Light Detector Task` and assign it to Core 0.
//          - Create `LCD Task` and assign it to Core 0.
//          - Create `Anomaly Alarm Task` and assign it to Core 1.
//          - Create `Prime Calculation Task` and assign it to Core 1.
  Serial.begin(115200);
  Wire.begin();
  
  lcd.init();
  lcd.backlight();
  lcd_command(0x01); // clears the LCD screen
  
  // Set up a binary semaphore used for signaling between tasks or interrupts and tasks.
  xSemaphore = xSemaphoreCreateBinary();
  if (xSemaphore != NULL) {
    xSemaphoreGive(xSemaphore); // init
  }

  // Create tasks and bound to core (Core 0 or Core 1)
  xTaskCreatePinnedToCore(LightDetectorTask, "Light Detector", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(LCDTask, "LCD Display", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(AnomalyAlarmTask, "Anomaly Alarm", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(PrimeCalculationTask, "Prime Calc", 2048, NULL, 1, NULL, 1);
}

void loop() {}
