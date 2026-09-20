#ifndef _GPIO_H
#define _GPIO_H

#include "stdint.h"
typedef struct {
  uint32_t MODER;
  uint32_t OTYPER;
  uint32_t OSPEEDR;
  uint32_t PUPDR;
  uint32_t IDR;
  uint32_t ODR;
  uint32_t BSRR;
  uint32_t LCKR;
  uint32_t AFR[2];
} GPIO_RegisterMap;

#define GPIOA ((GPIO_RegisterMap *)0x40020000)

#define SET_GPIO_MODE(gpio, function, pin_number)                              \
  gpio->MODER &= ~(0b11 << (2 * pin_number));                                  \
  gpio->MODER |= (function << (2 * pin_number));

#define SET_GPIO_AF(gpio, function, pin_number)                                \
  gpio->AFR[pin_number / 8] &= ~(0b1111 << (4 * pin_number));                  \
  gpio->AFR[pin_number / 8] |= (function << (4 * pin_number));

#define SET_GPIO_OTYPER(gpio, open_drain, pin_number)                          \
  gpio->OTYPER &= ~(1 << pin_number);                                          \
  gpio->OTYPER |= (open_drain << pin_number);

#define SET_GPIO_OSPEEDR(gpio, speed, pin_number)                              \
  gpio->OSPEEDR &= ~(0b11 << (2 * pin_number));                                \
  gpio->OSPEEDR |= (speed << (2 * pin_number));

#define SET_GPIO_PUPDR(gpio, pup, pin_number)                                  \
  gpio->PUPDR &= (0b11 << (2 * pin_number));                                   \
  gpio->PUPDR |= (pup << (2 * pin_number));

#define GPIO_MODE_AF 0b10

#define GPIO_OSPEEDR_MEDIUM 0b01
#define GPIO_OSPEEDR_FAST 0b10

#define GPIO_PUPDR_NOPP 0b00

#endif // !_GPIO_H_
