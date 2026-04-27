//Filename: Lab2Par1Step3_register.ino
//Author: Sung-Po Chang
//Date: 04/25/2026
//Description: Compare time between digitWrite function and manipulating hardware registers
//Version: 1.0

#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_periph.h"

#define GPIO_PIN 5 

void setup() {
  Serial.begin(115200);
  // Configure IO MUX to select GPIO function for the physical pin
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[GPIO_PIN], PIN_FUNC_GPIO);

  // Enable output driver for the specified GPIO pin
  *((volatile uint32_t *)GPIO_ENABLE_REG) |= (1 << GPIO_PIN);
}

void loop() {
  // Pointer to the GPIO Output Register for direct memory access
  volatile uint32_t* out_reg = (volatile uint32_t *)GPIO_OUT_REG;

  // Create a bitmask to isolate and target only the specified GPIO pin
  uint32_t bit_mask = (1 << GPIO_PIN);

  unsigned long startTime = micros();

  for (int i = 0; i < 1000; i++) {
    *out_reg |= bit_mask;   // HIGH
    *out_reg &= ~bit_mask;  // LOW
  }

  unsigned long endTime = micros();

  Serial.print("Register (1000): ");
  Serial.print(endTime - startTime);
  Serial.println(" us");

  delay(1000);
}