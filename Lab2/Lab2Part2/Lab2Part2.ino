//Filename: Lab2Par2.ino
//Author: Sung-Po Chang
//Date: 04/25/2026
//Description: Timer and Synchronization 
//Version: 1.0

#include "soc/timer_group_reg.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"

#define GPIO_PIN 5
unsigned long lastToggleTime = 0;
const unsigned long interval = 1000000;

void setup() {
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[GPIO_PIN], PIN_FUNC_GPIO);
  *((volatile uint32_t *)GPIO_ENABLE_REG) |= (1 << GPIO_PIN);

  // --- Timer ---
  // TIMG_T0CONFIG_REG(0)：
  // 1. Bit 31 (EN): 1
  // 2. Bit 30 (INCREASE): 1
  // 3. Bit 29 (AUTORELOAD): 1
  // 4. Bit 13-28 (DIVIDER): 80
  
  uint32_t config_val = 0;
  config_val |= (1UL << 31);          // Enable
  config_val |= (1UL << 30);          // Increase
  config_val |= (1UL << 29);          // Auto-reload
  config_val |= (80UL << 13);         // Prescaler = 80
  
  *((volatile uint32_t *)TIMG_T0CONFIG_REG(0)) = config_val;
}

void loop() {
  *((volatile uint32_t *)TIMG_T0UPDATE_REG(0)) = 1;

  uint32_t currentTime = *((volatile uint32_t *)TIMG_T0LO_REG(0));

  if (currentTime - lastToggleTime >= interval) {
    if (*((volatile uint32_t *)GPIO_OUT_REG) & (1 << GPIO_PIN)) {
      *((volatile uint32_t *)GPIO_OUT_REG) &= ~(1 << GPIO_PIN); // DOWN
    } else {
      *((volatile uint32_t *)GPIO_OUT_REG) |= (1 << GPIO_PIN);  // LIGHT
    }
    
    lastToggleTime = currentTime;
  }
}