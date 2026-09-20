#include "../../include/usart.h"
#include "../../include/gpio.h"
#include "../../include/nvic.h"
#include "../../include/rcc.h"
#include "../../include/ring_buffer.h"

#define RX_BUFFER_SIZE 16
#define TX_BUFFER_SIZE 16
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static ring_buffer_t rx_ring_buffer = {
    .buffer = rx_buffer, .size = RX_BUFFER_SIZE, .head = 0, .tail = 0};

static uint8_t tx_buffer[TX_BUFFER_SIZE];
static ring_buffer_t tx_ring_buffer = {
    .buffer = tx_buffer, .size = TX_BUFFER_SIZE, .head = 0, .tail = 0};

void usart2_handler(void) {
  if ((USART2->SR & USART_SR_TXE) && (USART2->CR1 & USART_ENABLE_TXEIE)) {
    if (!ring_buffer_empty(&tx_ring_buffer)) {
      USART2->DR = ring_buffer_get(&tx_ring_buffer);
    } else {
      USART2->CR1 &= ~USART_ENABLE_TXEIE;
    }
  }
  if (USART2->SR & USART_SR_RXNE) {
    if (!ring_buffer_full(&rx_ring_buffer)) {
      ring_buffer_put(&rx_ring_buffer, USART2->DR);
    }
  }
}

void usart2_init(void) {
  // enable clock for GPIOA on AHB1 and USART2 on APB1
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  uint8_t pin_pa2 = 2;
  uint8_t pin_pa3 = 3;
  // set the gpio mode to alternate function
  SET_GPIO_MODE(GPIOA, GPIO_MODE_AF, pin_pa2);
  SET_GPIO_MODE(GPIOA, GPIO_MODE_AF, pin_pa3);

  // no output drain
  SET_GPIO_OTYPER(GPIOA, 0, pin_pa2);
  SET_GPIO_OTYPER(GPIOA, 0, pin_pa3);

  // gpio speed to fast
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, pin_pa2);
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, pin_pa3);

  // no input pull up or pull down
  SET_GPIO_PUPDR(GPIOA, GPIO_PUPDR_NOPP, pin_pa2);
  SET_GPIO_PUPDR(GPIOA, GPIO_PUPDR_NOPP, pin_pa3);

  // usart2 alternate function is 7
  SET_GPIO_AF(GPIOA, 7, pin_pa2);
  SET_GPIO_AF(GPIOA, 7, pin_pa3);

  USART2->CR1 = 0;

  // baud rate calculation
  USART2->BRR = 0x016D;

  USART2->CR1 |= USART_ENABLE_TE | USART_ENABLE_RE | USART_ENABLE_RXNEIE;

  USART2->CR1 |= USART_ENABLE;

  NVIC_ENABLE_IRQ(USART2_IRQ);
}

// blocks while there is no space in tx_ring_buffer
void usart2_putc(char c) {
  while (ring_buffer_full(&tx_ring_buffer))
    ;

  ring_buffer_put(&tx_ring_buffer, (uint8_t)c);

  USART2->CR1 |= USART_ENABLE_TXEIE;
}

void usart2_puts(const char *str) {
  while (*str)
    usart2_putc(*str++);
}

bool usart2_getc(char *c) {
  if (ring_buffer_empty(&rx_ring_buffer))
    return false;

  *c = ring_buffer_get(&rx_ring_buffer);

  return true;
}
