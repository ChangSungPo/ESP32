/**
 * @file Lab4part1.ino
 * @author Sung-Po Chang
 * @brief Preemptive Shortest Remaining Time First (SRTF) Scheduler Implementation.
 * @version 1.0
 * @date 2026-05-25
 * * @mainpage Laboratory 4 Part 1: Preemptive SRTF Scheduler
 * * @section intro_sec Introduction
 * This project implements a custom, software-driven preemptive Shortest Remaining Time First 
 * (SRTF) scheduler on top of FreeRTOS running on an ESP32. The central scheduler task manages 
 * three concurrent worker threads by controlling their states using time-slice slices.
 * * @section modules_sec Main Functional Modules
 * - Task Preemption Control (via scheduleTasks)
 * - Custom Peripheral Interface (Bit-masked I2C LCD Driver)
 * - Worker Routines (LED Blinker, LCD Counter, and Serial Alphabet Printer)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/** @brief Bit 0: Register Select bit (0 = command, 1 = data). */
#define BIT_RS        0x01  
/** @brief Bit 1: Read/Write bit (0 = writing to LCD, 1 = reading from LCD). */
#define BIT_RW        0x02  
/** @brief Bit 2: Enable bit (HIGH to latch incoming data/command, LOW to process). */
#define BIT_EN        0x04  
/** @brief Bit 3: Backlight control bit (1 = turn on, 0 = turn off). */
#define BIT_BACKLIGHT 0x08  

/** @brief Digital pin connected to the onboard indicator LED. */
#define LED_BLINK_PIN  4    

/** @brief Global I2C LCD object instantiation. */
LiquidCrystal_I2C lcd(0x27, 16, 2); 

/** @brief Global task handle for the LED Blinker routine. */
TaskHandle_t ledTaskHandle = NULL;
/** @brief Global task handle for the LCD Counter routine. */
TaskHandle_t counterTaskHandle = NULL;
/** @brief Global task handle for the Serial Alphabet Printer routine. */
TaskHandle_t alphabetTaskHandle = NULL;

void ledTask(void *arg);
void counterTask(void *arg);
void alphabetTask(void *arg);
void scheduleTasks(void *arg);

/** @brief Total required computation ticks for the LED task (500 ms). */
const TickType_t ledTaskExecutionTime = 500 / portTICK_PERIOD_MS;      
/** @brief Total required computation ticks for the Counter task (2000 ms). */
const TickType_t counterTaskExecutionTime = 2000 / portTICK_PERIOD_MS; 
/** @brief Total required computation ticks for the Alphabet task (13000 ms). */
const TickType_t alphabetTaskExecutionTime = 13000 / portTICK_PERIOD_MS; 

/** @brief Volatile counter tracking the remaining execution ticks of the LED task. */
volatile TickType_t remainingLedTime = ledTaskExecutionTime;
/** @brief Volatile counter tracking the remaining execution ticks of the Counter task. */
volatile TickType_t remainingCounterTime = counterTaskExecutionTime;
/** @brief Volatile counter tracking the remaining execution ticks of the Alphabet task. */
volatile TickType_t remainingAlphabetTime = alphabetTaskExecutionTime;

/**
 * @brief Low-level driver to process and transmit raw command or data bytes to the I2C LCD.
 * @details Splits the 8-bit input into high and low nibbles to match the 4-bit transfer protocol.
 * @param value The 8-bit packet containing data characters or device configurations.
 * @param is_data Context flag set to true for RAM data writing, false for register commands.
 */
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

  // 2. second half data (Bits 0-3)
  uint8_t low_nibble = (value << 4) & 0xF0;
  Wire.beginTransmission(0x27);
  Wire.write(low_nibble | backlight_bit | BIT_EN | rs_bit);
  Wire.write(low_nibble | backlight_bit | 0x00 | rs_bit);
  Wire.endTransmission();
  delay(1);
}

/**
 * @brief Sends an instruction command to configure the LCD screen layout or clear status.
 * @param cmd The 8-bit hex configuration code.
 */
void lcd_command(uint8_t cmd) {
  lcd_send(cmd, false);
}

/**
 * @brief Transmits a standalone raw ASCII character byte to be printed at the active cursor.
 * @param data The text symbol byte.
 */
void lcd_data(uint8_t data) {
  lcd_send(data, true);
}

/**
 * @brief Parses and prints a null-terminated string onto the display.
 * @param str Constant character pointer to the target message string.
 */
void lcd_print(const char* str) {
  while (*str) {
    lcd_data(*str++);
  }
}

/**
 * @brief Formats and renders a multi-digit integer to the display.
 * @param num The numerical integer to display.
 */
void lcd_print_int(int num) {
  if (num >= 10) {
    lcd_data('0' + (num / 10));
    lcd_data('0' + (num % 10));
  } else {
    lcd_data('0' + num);
  }
}

/**
 * @brief Worker Task 1: Periodic LED Blinker loop.
 * @param arg Pointer to task parameters passed during initialization (Unused).
 */
void ledTask(void *arg) {
  bool ledState = LOW;
  while (1) {
    ledState = !ledState;
    digitalWrite(LED_BLINK_PIN, ledState);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Worker Task 2: Incremental LCD numerical Counter display loop.
 * @param arg Pointer to task parameters passed during initialization (Unused).
 */
void counterTask(void *arg) {
  int count = 1;
  while (1) {
    lcd_command(0x01); 
    delay(2);
    
    lcd_command(0x80);
    lcd_print("Count: "); 
    lcd_print_int(count);
    
    count++;
    if (count > 20) count = 1;
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

/**
 * @brief Worker Task 3: Sequential A-Z Alphabet Printer loop.
 * @param arg Pointer to task parameters passed during initialization (Unused).
 */
void alphabetTask(void *arg) {
  char c = 'A';
  int quantumCounter = 0;
  while (1) {
    if (quantumCounter == 0) {
      Serial.print(c);
      if (c < 'Z') {
        Serial.print(", ");
      } else {
        Serial.println();
      }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    quantumCounter++;
    if (quantumCounter >= 5) { 
      quantumCounter = 0;
      c++;
      if (c > 'Z') {
        c = 'A';
      } 
    }
  }
}

/**
 * @brief Core SRTF Central Scheduler Task.
 * @details Evaluates all actively available tasks, dispatches the one with the shortest 
 * remaining run requirement for a 100ms slice, tracks budgets, and monitors cooldown cycles.
 * @param arg Pointer to task parameters passed during initialization (Unused).
 */
void scheduleTasks(void *arg) {
  vTaskSuspend(ledTaskHandle);
  vTaskSuspend(counterTaskHandle);
  vTaskSuspend(alphabetTaskHandle);

  const TickType_t timeQuantum = 100 / portTICK_PERIOD_MS;

  const TickType_t ledArrivalPeriod = 4000 / portTICK_PERIOD_MS;
  const TickType_t counterArrivalPeriod = 8000 / portTICK_PERIOD_MS;
  const TickType_t alphabetArrivalPeriod = 25000 / portTICK_PERIOD_MS;

  TickType_t ledNextArrival = xTaskGetTickCount();
  TickType_t counterNextArrival = xTaskGetTickCount();
  TickType_t alphabetNextArrival = xTaskGetTickCount();

  while (1) {
    TickType_t currentTick = xTaskGetTickCount();

    if (remainingLedTime == 0 && currentTick >= ledNextArrival) {
      remainingLedTime = ledTaskExecutionTime;
      Serial.println("\nLED Task re-arrived into system.");
    }

    if (remainingCounterTime == 0 && currentTick >= counterNextArrival) {
      remainingCounterTime = counterTaskExecutionTime;
      Serial.println("\nCounter Task re-arrived into system.");
    }

    if (remainingAlphabetTime == 0 && currentTick >= alphabetNextArrival) {
      remainingAlphabetTime = alphabetTaskExecutionTime;
      Serial.println("\nAlphabet Task re-arrived into system.");
    }

    TickType_t minRemainingTime = portMAX_DELAY;
    TaskHandle_t nextTask = NULL;
    int selectedTaskNum = 0;

    if (remainingLedTime > 0 && remainingLedTime < minRemainingTime) {
      minRemainingTime = remainingLedTime;
      nextTask = ledTaskHandle;
      selectedTaskNum = 1;
    }
    if (remainingCounterTime > 0 && remainingCounterTime < minRemainingTime) {
      minRemainingTime = remainingCounterTime;
      nextTask = counterTaskHandle;
      selectedTaskNum = 2;
    }
    if (remainingAlphabetTime > 0 && remainingAlphabetTime < minRemainingTime) {
      minRemainingTime = remainingAlphabetTime;
      nextTask = alphabetTaskHandle;
      selectedTaskNum = 3;
    }

    if (nextTask != NULL) {
      vTaskResume(nextTask);
      vTaskDelay(timeQuantum);        
      vTaskSuspend(nextTask);

      if (selectedTaskNum == 1) {
        if (remainingLedTime >= timeQuantum) {
          remainingLedTime -= timeQuantum;
        } else {
          remainingLedTime = 0;
        }

        if (remainingLedTime == 0) { 
          ledNextArrival = xTaskGetTickCount() + ledArrivalPeriod;
          Serial.println("\nLED Task finished! Entering cooldown...");
        }
      } 
      else if (selectedTaskNum == 2) { 
        if (remainingCounterTime >= timeQuantum) {
          remainingCounterTime -= timeQuantum;
        } else {
          remainingCounterTime = 0;
        }

        if (remainingCounterTime == 0) { 
          counterNextArrival = xTaskGetTickCount() + counterArrivalPeriod;
          Serial.println("\nCounter Task finished! Entering cooldown...");
        }
      } 
      else if (selectedTaskNum == 3) {
        if (remainingAlphabetTime >= timeQuantum) {
          remainingAlphabetTime -= timeQuantum;
        } else {
          remainingAlphabetTime = 0;
        }

        if (remainingAlphabetTime == 0) { 
          alphabetNextArrival = xTaskGetTickCount() + alphabetArrivalPeriod;
          Serial.println("\nAlphabet Task finished! Entering cooldown...");
        }
      }
    } else {
      vTaskDelay(timeQuantum);
    }
  }
}

/**
 * @brief Arduino hardware setup hook.
 * @details Initializes peripheral pins, UART interfaces, and spins up the multi-task threads.
 */
void setup() {
  Serial.begin(115200);
  Wire.begin();         
  lcd.init();           
  delay(2);             

  pinMode(LED_BLINK_PIN, OUTPUT); 

  Serial.println("--- Scheduler Initialized with Manual LCD Driver ---");

  xTaskCreatePinnedToCore(ledTask, "LED Task", 2048, NULL, 1, &ledTaskHandle, 0);
  xTaskCreatePinnedToCore(counterTask, "Counter Task", 2048, NULL, 1, &counterTaskHandle, 0);
  xTaskCreatePinnedToCore(alphabetTask, "Alphabet Task", 2048, NULL, 1, &alphabetTaskHandle, 0);
  xTaskCreatePinnedToCore(scheduleTasks, "SRTF Scheduler", 2048, NULL, 2, NULL, 0);
}

/**
 * @brief Standard Arduino main loop entry point (Unused in FreeRTOS environment).
 */
void loop() {
  // Empty loop
}