#ifndef _SPI_H
#define _SPI_H

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

void spi1_cs_select(void);
void spi1_cs_deselect(void);

void spi1_init(void);
uint8_t spi1_transfer(uint8_t data);
void spi1_write(const uint8_t *data, uint32_t len);
void spi1_read(uint8_t *data, uint32_t len);
void spi1_transfer_buffer(const uint8_t *tx, uint8_t *rx, uint32_t len);

#endif
