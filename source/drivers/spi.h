#ifndef _SPI_H
#define _SPI_H

#include "stdbool.h"
#include "stdint.h"

typedef struct {
  uint32_t CR1;
  uint32_t CR2;
  uint32_t SR;
  uint32_t DR;
  uint32_t CRCPR;
  uint32_t RXCRCR;
  uint32_t TXCRCR;
  uint32_t I2SCFGR;
  uint32_t I2SPR;
} SPI_RegisterMap;

#define SPI1 ((SPI_RegisterMap *)0x40013000)

#define SPI_CR1_MSTR (1 << 2)
#define SPI_CR1_BR_DIV8 (0b010 << 3)
#define SPI_CR1_SPE (1 << 6)
#define SPI_CR1_SSI (1 << 8)
#define SPI_CR1_SSM (1 << 9)

#define SPI_SR_RXNE (1 << 0)
#define SPI_SR_TXE (1 << 1)

#define SPI1_PIN_CS 4
#define SPI1_PIN_SCK 5
#define SPI1_PIN_MISO 6
#define SPI1_PIN_MOSI 7
#define SPI1_AF 5

typedef struct {
  const uint8_t *header;
  uint8_t header_len;

  const uint8_t *tx_data;
  uint8_t *rx_data;
  uint32_t data_len;

  bool is_read;
} spi1_frame_t;

typedef enum { SPI_OK, SPI_ERR_TXE, SPI_ERR_RXE } spi_return_t;

void spi1_init(void);
spi_return_t spi1_transfer_frame(spi1_frame_t *frame);

#endif
