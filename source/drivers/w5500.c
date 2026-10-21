#include "w5500.h"
#include "../../include/gpio.h"
#include "../../include/kprintf.h"
#include "../../include/rcc.h"
#include "../../include/syscall.h"
#include "spi.h"

#define W5500_RESET_PIN 1

#define W5500_BSB_COMMON 0x00

#define W5500_CTRL_RWB_READ (0 << 2)
#define W5500_CTRL_RWB_WRITE (1 << 2)
#define W5500_CTRL_OM_VDM (0b00 << 0)

static spi_return_t w5500_transfer(uint16_t addr, uint8_t bsb,
                                   spi1_frame_t *frame) {
  uint8_t control =
      (bsb << 3) | W5500_CTRL_OM_VDM |
      (frame->is_read == true ? W5500_CTRL_RWB_READ : W5500_CTRL_RWB_WRITE);
  uint8_t h[3];
  h[0] = (addr >> 8) & 0xFF;
  h[1] = (addr & 0xFF);
  h[2] = control;

  spi1_frame_t my_frame = {.header = h,
                           .header_len = 3,
                           .tx_data = frame->tx_data,
                           .rx_data = frame->rx_data,
                           .is_read = frame->is_read,
                           .data_len = frame->data_len};

  return spi1_transfer_frame(&my_frame);
}

static spi_return_t w5500_read8(uint16_t addr, uint8_t bsb, uint8_t *data) {
  spi1_frame_t frame = {.is_read = true, .rx_data = data, .data_len = 1};
  return w5500_transfer(addr, bsb, &frame);
}

static spi_return_t w5500_write8(uint16_t addr, uint8_t bsb, uint8_t v) {
  spi1_frame_t frame = {.is_read = false, .tx_data = &v, .data_len = 1};
  return w5500_transfer(addr, bsb, &frame);
}

static spi_return_t w5500_read16(uint16_t addr, uint8_t bsb, uint16_t *data) {
  uint8_t v[2] = {0, 0};
  spi1_frame_t frame = {.is_read = true, .rx_data = v, .data_len = 2};
  spi_return_t ret = w5500_transfer(addr, bsb, &frame);
  *data = ((uint16_t)v[0] << 8) | v[1];
  return ret;
}

static spi_return_t w5500_read16_stable(uint16_t addr, uint8_t bsb,
                                        uint16_t *data) {
  uint16_t a, b;
  uint8_t iterations = 0;
  spi_return_t ret = SPI_OK;
  do {
    ret = w5500_read16(addr, bsb, &a);
    if (ret != SPI_OK)
      break;
    ret = w5500_read16(addr, bsb, &b);
    if (ret != SPI_OK)
      break;
  } while (a != b && ++iterations < 255);
  if (iterations == 255)
    return SPI_ERR_RXE;
  *data = a;
  return ret;
}

static spi_return_t w5500_write16(uint16_t addr, uint8_t bsb, uint16_t data) {
  uint8_t v[2];
  v[0] = (uint8_t)(data >> 8);
  v[1] = (uint8_t)(data & 0x00FF);
  spi1_frame_t frame = {.is_read = false, .tx_data = v, .data_len = 2};
  return w5500_transfer(addr, bsb, &frame);
}

static void w5500_reset_pin_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  SET_GPIO_MODE(GPIOA, GPIO_MODE_OUTPUT, W5500_RESET_PIN);
  SET_GPIO_OTYPER(GPIOA, 0, W5500_RESET_PIN);
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, W5500_RESET_PIN);
  GPIO_PIN_HIGH(GPIOA, W5500_RESET_PIN); // idle high
}

static void w5500_hw_reset(void) {
  GPIO_PIN_LOW(GPIOA, W5500_RESET_PIN);
  sys_sleep(1);
  GPIO_PIN_HIGH(GPIOA, W5500_RESET_PIN);
  sys_sleep(50);
}

#define INIT_ATTEMPTS 5

void w5500_init(const uint8_t mac[6], const uint8_t ip[4],
                const uint8_t gateway[4], const uint8_t subnet[4]) {
  uint8_t attempts = 0;
  uint8_t version = 0;
  spi_return_t ret = SPI_OK;
  do {
    w5500_reset_pin_init();
    w5500_hw_reset();
    ret = w5500_get_version(&version);
    if (ret != SPI_OK)
      kprintf("[w5500] SPI error: %d\r\n", ret);
    kprintf("[w5500] trying to start, attempt %d\r\n", attempts);
  } while (version != 0x04 && ++attempts < INIT_ATTEMPTS);

  kprintf("[w5500] version %d\r\n", version);
  if (attempts == INIT_ATTEMPTS) {
    kprintf("[!][w5500] couldn't start\r\n");
    return;
  }

  spi1_frame_t mac_frame = {.is_read = false, .tx_data = mac, .data_len = 6};
  spi1_frame_t ip_frame = {.is_read = false, .tx_data = ip, .data_len = 4};
  spi1_frame_t gateway_frame = {
      .is_read = false, .tx_data = gateway, .data_len = 4};
  spi1_frame_t subnet_frame = {
      .is_read = false, .tx_data = subnet, .data_len = 4};
  ret = w5500_transfer(W5500_SHAR, W5500_BSB_COMMON, &mac_frame);
  ret = w5500_transfer(W5500_SIPR, W5500_BSB_COMMON, &ip_frame);
  ret = w5500_transfer(W5500_GAR, W5500_BSB_COMMON, &gateway_frame);
  ret = w5500_transfer(W5500_SUBR, W5500_BSB_COMMON, &subnet_frame);
}

spi_return_t w5500_get_version(uint8_t *data) {
  return w5500_read8(W5500_VERSIONR, W5500_BSB_COMMON, data);
}

spi_return_t w5500_link_up(int *up) {
  uint8_t data = 0;
  spi_return_t ret = w5500_read8(W5500_PHYCFGR, W5500_BSB_COMMON, &data);
  if (ret == SPI_OK)
    *up = (data & (0x01));
  else
    *up = -1;

  return ret;
}

#define OPEN_MAX_TRIES 5
#define CLOSE_MAX_TRIES 5

void w5500_socket_close(socket_t *socket) {
  uint8_t tries = 0;

  w5500_write8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number), SOCKET_CMD_CLOSE);

  uint8_t current_sr = 1;
  do {
    sys_sleep(1);
    w5500_read8(SOCKET_SR, W5500_BSB_SOCK_REG(socket->number), &current_sr);
  } while (current_sr != SOCK_CLOSED && ++tries < CLOSE_MAX_TRIES);

  if (tries == CLOSE_MAX_TRIES) {
    kprintf("[!][w5500] couldn't close socket %d\r\n", socket->number);
    socket->status = SOCKET_STATUS_ERROR;
    return;
  }

  kprintf("[w5500] socket %d closed\r\n", socket->number);
  socket->status = SOCKET_STATUS_CLOSED;
}

void w5500_socket_open(socket_t *socket) {
  if (socket->number > 7) {
    socket->status = SOCKET_STATUS_ERROR;
    return;
  }

  // defensive close
  w5500_socket_close(socket);
  if (socket->status == SOCKET_STATUS_ERROR)
    return;

  uint8_t mode = 0;
  mode |= socket->protocol;
  w5500_write8(SOCKET_MR, W5500_BSB_SOCK_REG(socket->number), mode);
  w5500_write16(SOCKET_PORT, W5500_BSB_SOCK_REG(socket->number), socket->port);

  w5500_write8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number), SOCKET_CMD_OPEN);

  uint8_t wanted_sr = 0;
  if (socket->protocol == TCP) {
    wanted_sr = SOCKET_SR_INIT;
  } else if (socket->protocol == UDP) {
    wanted_sr = SOCKET_SR_UDP;
  }

  uint8_t current_sr = 0;
  uint8_t tries = 0;
  do {
    sys_sleep(2);
    w5500_read8(SOCKET_SR, W5500_BSB_SOCK_REG(socket->number), &current_sr);
  } while (wanted_sr != current_sr && ++tries < OPEN_MAX_TRIES);

  if (tries == OPEN_MAX_TRIES) {
    kprintf("[!][w5500] couldn't open socket %d, protocol %d\r\n",
            socket->number, socket->protocol);
    socket->status = SOCKET_STATUS_ERROR;
    return;
  }

  uint8_t current_mr = 0;
  uint16_t current_port = 0;
  w5500_read8(SOCKET_MR, W5500_BSB_SOCK_REG(socket->number), &current_mr);
  w5500_read16(SOCKET_PORT, W5500_BSB_SOCK_REG(socket->number), &current_port);

  if (current_mr != mode) {
    kprintf("[!][w5500] mr mismatch %d != %d\r\n", current_mr, mode);
    socket->status = SOCKET_STATUS_ERROR;
    return;
  }
  if (current_port != socket->port) {
    kprintf("[!][w5500] port mismatch %d != %d\r\n", current_port,
            socket->port);
    socket->status = SOCKET_STATUS_ERROR;
    return;
  }

  kprintf("[w5500] socket %d opened\r\n", socket->number);

  socket->status = SOCKET_STATUS_OPEN;

  return;
}

typedef struct {
  uint8_t src_ip[4];
  uint16_t src_port;
  uint16_t length;
} udp_header_t;

static w5500_recv_status_t w5500_recv_udp_header(socket_t *socket,
                                                 udp_header_t *header,
                                                 uint16_t *read_pointer) {
  w5500_read16(SOCKET_RX_RD, W5500_BSB_SOCK_REG(socket->number), read_pointer);
  uint8_t buffer[8];
  spi1_frame_t frame = {.rx_data = buffer, .data_len = 8, .is_read = true};
  spi_return_t ret = w5500_transfer(*read_pointer, _RX(socket->number), &frame);
  if (ret != SPI_OK) {
    return W5500_RECV_ERR;
  }
  header->src_ip[0] = buffer[0];
  header->src_ip[1] = buffer[1];
  header->src_ip[2] = buffer[2];
  header->src_ip[3] = buffer[3];
  header->src_port = (buffer[4] << 8) | buffer[5];
  header->length = (buffer[6] << 8) | buffer[7];

  return W5500_RECV_OK;
}

// recevies data from the socket of length bounded by buf_len
// returns the length of data left over or -1 if there is nothing to read
w5500_recv_status_t w5500_socket_recv(socket_t *socket, uint8_t *buffer,
                                      uint16_t maxlen, uint16_t *got,
                                      uint8_t src_ip[4], uint16_t *src_port) {
  uint16_t recv_len = 0;
  spi_return_t ret = SPI_OK;
  ret = w5500_read16_stable(SOCKET_RX_RSR, W5500_BSB_SOCK_REG(socket->number),
                            &recv_len);
  if (ret != SPI_OK) {
    return W5500_RECV_ERR;
  }
  if (recv_len == 0) {
    *got = 0;
    return W5500_RECV_EMPTY;
  }
  udp_header_t header = {0, 0, 0};
  uint16_t read_pointer = 0;
  if (w5500_recv_udp_header(socket, &header, &read_pointer) == W5500_RECV_ERR) {
    *got = 0;
    return W5500_RECV_ERR;
  }
  read_pointer += 8; // because of the header

  uint16_t length_to_read = header.length < maxlen ? header.length : maxlen;

  // read max possible from spi
  spi1_frame_t frame = {
      .rx_data = buffer, .data_len = length_to_read, .is_read = true};
  ret = w5500_transfer(read_pointer, _RX(socket->number), &frame);
  if (ret != SPI_OK) {
    *got = 0;
    return W5500_RECV_ERR;
  }
  *got = length_to_read;
  for (int i = 0; i < 4; i++) {
    *(src_ip + i) = header.src_ip[i];
  }
  *src_port = header.src_port;

  // write new RX_RD which is old_RX_RD += len of header + len of data
  // regardless of if we read the data fully
  w5500_write16(SOCKET_RX_RD, W5500_BSB_SOCK_REG(socket->number),
                read_pointer + header.length);
  w5500_write8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number), SOCKET_CMD_RECV);

  if (header.length > maxlen) {
    return W5500_RECV_TRUNCATED;
  } else {
    return W5500_RECV_OK;
  }
}
