#include "../../include/usart.h"
#include "../../include/gpio.h"
#include "../../include/rcc.h"

void usart2_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  uint8_t pin_pa2 = 2;
  uint8_t pin_pa3 = 3;
  SET_GPIO_MODE(GPIOA, GPIO_MODE_AF, pin_pa2);
  SET_GPIO_MODE(GPIOA, GPIO_MODE_AF, pin_pa3);

  SET_GPIO_OTYPER(GPIOA, 0, pin_pa2);
  SET_GPIO_OTYPER(GPIOA, 0, pin_pa3);

  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, pin_pa2);
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, pin_pa3);

  SET_GPIO_PUPDR(GPIOA, GPIO_PUPDR_NOPP, pin_pa2);
  SET_GPIO_PUPDR(GPIOA, GPIO_PUPDR_NOPP, pin_pa3);

  SET_GPIO_AF(GPIOA, 7, pin_pa2);
  SET_GPIO_AF(GPIOA, 7, pin_pa3);

  USART2->CR1 = 0;

  USART2->BRR = 0x016D;

  USART2->CR1 |= USART_ENABLE_TE | USART_ENABLE_RE;

  USART2->CR1 |= USART_ENABLE;
}

void usart2_putc(char c) {
  while (!(USART2->SR & USART_SR_TXE))
    ;

  USART2->DR = (uint8_t)c;
}

void usart2_puts(const char *str) {
  while (*str)
    usart2_putc(*str++);
}

void usart2_test(void) {
  while (1) {
    while (!(USART2->SR & USART_SR_TXE))
      ;

    USART2->DR = 0x55;
  }
}
