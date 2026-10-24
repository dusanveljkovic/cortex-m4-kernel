#include "spi.h"
#include "../../include/gpio.h"
#include "../../include/rcc.h"
#include "../../include/semaphore.h"
#include "../../include/syscall.h"

#define SPI_SPIN_LIMIT 100

static void spi1_cs_select(void) { GPIO_PIN_LOW(GPIOA, SPI1_PIN_CS); }
static void spi1_cs_deselect(void) { GPIO_PIN_HIGH(GPIOA, SPI1_PIN_CS); }

static mutex_t spi1_lock = {0, 0, 0, 0};
static spi_state_t state = SPI_ST_UNINIT;

/* Configure the needed registers (RCC, GPIO, SPI1)
 * Returns SPI_ERR_ARG when called from state that is not SPI_ST_UNINIT
 */
spi_return_t spi1_configure(void) {
  if (state != SPI_ST_UNINIT) {
    return SPI_ERR_ARG;
  }
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

  SET_GPIO_MODE(GPIOA, GPIO_MODE_OUTPUT, SPI1_PIN_CS);
  SET_GPIO_OTYPER(GPIOA, 0, SPI1_PIN_CS);
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, SPI1_PIN_CS);
  spi1_cs_deselect();

  uint8_t pins[] = {SPI1_PIN_SCK, SPI1_PIN_MISO, SPI1_PIN_MOSI};
  for (int i = 0; i < 3; i++) {
    SET_GPIO_MODE(GPIOA, GPIO_MODE_AF, pins[i]);
    SET_GPIO_OTYPER(GPIOA, 0, pins[i]);
    SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, pins[i]);
    SET_GPIO_PUPDR(GPIOA, GPIO_PUPDR_NOPP, pins[i]);
    SET_GPIO_AF(GPIOA, SPI1_AF, pins[i])
  }

  SPI1->CR1 = 0;
  SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_DIV8 // 84MHz / 8 = 10.5 MHz clock
              | SPI_CR1_SSM | SPI_CR1_SSI;

  SPI1->CR2 = 0;

  SPI1->CR1 |= SPI_CR1_SPE;

  state = SPI_ST_CONFIGURED;
  return SPI_OK;
}

/* Initializes the mutex and sets the state to SPI_ST_READY so other SPI
 * function can work Return SPI_ERR_ARG when called with state that is not
 * SPI_ST_CONFIGURED
 */
spi_return_t spi1_start(void) {
  if (state != SPI_ST_CONFIGURED)
    return SPI_ERR_ARG;
  mutex_init(&spi1_lock);
  state = SPI_ST_READY;
  return SPI_OK;
}

/* Used to recover SPI from a fault by reconfiguring it. Doesn't reinitialize
 * the mutex Return SPI_ERR_ARG when called with state that is not
 * SPI_ST_FAULTED
 */
spi_return_t spi1_recover(void) {
  if (state != SPI_ST_FAULTED)
    return SPI_ERR_ARG;
  sys_mutex_lock(&spi1_lock);

  state = SPI_ST_UNINIT;
  spi1_configure();
  volatile uint8_t drain = (uint8_t)SPI1->DR;
  state = SPI_ST_READY;

  sys_mutex_unlock(&spi1_lock);
  return SPI_OK;
}

/*
 * Transfers a byte using SPI shift registers. First waits for a TX slot, then
 * sends a byte and waits for a reply.
 * Returns SPI_ERR_TIMEOUT when the wait for tx slot or rx data exceeds the
 * limit
 */
static spi_return_t spi1_transfer(uint8_t data, uint8_t *received) {
  uint32_t spins = 0;
  while (!(SPI1->SR & SPI_SR_TXE)) {
    if (++spins >= SPI_SPIN_LIMIT) {
      return SPI_ERR_TIMEOUT;
    }
  }
  spins = 0;

  SPI1->DR = data;

  while (!(SPI1->SR & SPI_SR_RXNE)) {
    if (++spins >= SPI_SPIN_LIMIT) {
      // drain the stale byte before returning
      (void)SPI1->DR;
      return SPI_ERR_TIMEOUT;
    }
  }

  uint8_t spi_dr = (uint8_t)SPI1->DR;
  if (received != 0)
    *received = spi_dr;
  return SPI_OK;
}

/* Transfers a whole spi1_frame_t
 * Locks the mutex, activates CS, transfers the header, then either transfers
 * the tx_data or gets data from the peripheral into rx_data Returns SPI_ERR_ARG
 * when SPI is uninitialized or the frame is not proper Returns SPI_ERR_FAULT
 * when state is FAULTED Can set state to SPI_ST_FAULTED when any byte transfer
 * fails
 */
spi_return_t spi1_transfer_frame(spi_frame_t *frame) {
  if (state == SPI_ST_UNINIT)
    return SPI_ERR_ARG;
  if (frame == 0)
    return SPI_ERR_ARG;
  // header has length but there is no header
  if (frame->header_len > 0 && frame->header == 0)
    return SPI_ERR_ARG;

  // data has length but there is no data
  if (frame->data_len > 0 && ((frame->is_read && frame->rx_data == 0) ||
                              (!frame->is_read && frame->tx_data == 0)))
    return SPI_ERR_ARG;

  if (state == SPI_ST_FAULTED)
    return SPI_ERR_FAULT;
  spi_return_t transfer_ret = SPI_OK;

  sys_mutex_lock(&spi1_lock);
  // faulted can be latched by another task so should be checked again
  if (state == SPI_ST_FAULTED) {
    sys_mutex_unlock(&spi1_lock);
    return SPI_ERR_FAULT;
  }
  if (SPI1->SR & SPI_SR_RXNE)
    (void)SPI1->DR;
  spi1_cs_select();

  for (uint32_t i = 0; i < frame->header_len; i++) {
    transfer_ret = spi1_transfer(frame->header[i], 0);
    if (transfer_ret != SPI_OK)
      break;
  }

  if (transfer_ret == SPI_OK) {
    for (uint32_t i = 0; i < frame->data_len; i++) {
      uint8_t received;
      transfer_ret =
          spi1_transfer(frame->is_read ? 0xFF : frame->tx_data[i], &received);
      if (transfer_ret != SPI_OK)
        break;

      if (frame->is_read && frame->rx_data)
        frame->rx_data[i] = received;
    }
  }

  spi1_cs_deselect();
  if (transfer_ret == SPI_ERR_TIMEOUT)
    state = SPI_ST_FAULTED;

  sys_mutex_unlock(&spi1_lock);

  return transfer_ret;
}
