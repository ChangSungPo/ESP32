//Filename: Lab2Par1Step2.ino
//Author: Sung-Po Chang
//Date: 04/25/2026
//Description: GPIO Control via Registers
//Version: 1.0

#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_periph.h"

#define GPIO_PIN 5 // Pin D2

void setup() {
  // put your setup code here, to run once:
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[GPIO_PIN], PIN_FUNC_GPIO);

  *((volatile uint32_t *)GPIO_ENABLE_REG) |= (1 << GPIO_PIN);
}

void loop() {
  // put your main code here, to run repeatedly:
  *((volatile uint32_t *)GPIO_OUT_REG) |= (1 << GPIO_PIN);
  delay(1000);

  *((volatile uint32_t *)GPIO_OUT_REG) &= ~(1 << GPIO_PIN);
  delay(1000);
}
