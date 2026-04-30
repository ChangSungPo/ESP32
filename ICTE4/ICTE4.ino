#include "soc/timer_group_reg.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"

// 1. state
#define STATE_READY    1
#define STATE_SLEEPING 2

// 2. TCB
typedef struct TCBstruct {
  void (*ftpr)(void *p);     // Function pointer
  void *arg_ptr;             // Argument pointer
  unsigned short int state;  // Task state
  unsigned int delay;        // Task delay (us)
  unsigned int lastRun;      // Internal tracking for timing
} TCB;

#define LED1_PIN 1
#define LED2_PIN 2
#define POT_PIN 7

int argLED1 = LED1_PIN;
int argLED2 = LED2_PIN;
int argPot  = POT_PIN;

void taskFlashLED(void *p) {
  int pin = *((int *)p); 
  *((volatile uint32_t *)GPIO_OUT_REG) ^= (1 << pin);
}

void taskReadPot(void *p) {
  int pin = *((int *)p);
  Serial.printf("Potentiometer (GPIO %d): %d\n", pin, analogRead(pin));
}

#define NTASKS 3
TCB TaskList[NTASKS] = {
  {taskFlashLED, &argLED1, STATE_READY, 500000, 0}, // Task 1: 0.5s
  {taskFlashLED, &argLED2, STATE_READY, 300000, 0}, // Task 2: 0.3s
  {taskReadPot,  &argPot,  STATE_READY, 1000000, 0} // Task 3: 1.0s
};

void setup() {
  Serial.begin(115200);

  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[LED1_PIN], PIN_FUNC_GPIO);
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[LED2_PIN], PIN_FUNC_GPIO);
  *((volatile uint32_t *)GPIO_ENABLE_REG) |= (1 << LED1_PIN) | (1 << LED2_PIN);

  // Hardware Timer Setup
  uint32_t config = (1UL << 31) | (1UL << 30) | (1UL << 29) | (80UL << 13);
  *((volatile uint32_t *)TIMG_T0CONFIG_REG(0)) = config;

  Serial.println("Round Robin Scheduler Initialized.");
}

uint32_t getHardwareTime() {
  *((volatile uint32_t *)TIMG_T0UPDATE_REG(0)) = 1;
  return *((volatile uint32_t *)TIMG_T0LO_REG(0));
}

void loop() {
  uint32_t currentTime = getHardwareTime();

  for (int i = 0; i < NTASKS; i++) {
    if (TaskList[i].state == STATE_READY) {
      if (currentTime - TaskList[i].lastRun >= TaskList[i].delay) {
        
        TaskList[i].ftpr(TaskList[i].arg_ptr);

        TaskList[i].lastRun = currentTime;
      }
    }
  }
}