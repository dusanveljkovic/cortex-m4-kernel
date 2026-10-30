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

/*
 * Builds a w5500 header with supplied BSB and address then sends
 * it through spi.
 * Maps the spi status into w5500 status are returns it
 */
static w5500_status_t w5500_transfer(uint16_t addr, uint8_t bsb,
                                     spi_frame_t *frame) {
  uint8_t control =
      (bsb << 3) | W5500_CTRL_OM_VDM |
      (frame->is_read == true ? W5500_CTRL_RWB_READ : W5500_CTRL_RWB_WRITE);
  uint8_t h[3];
  h[0] = (addr >> 8) & 0xFF;
  h[1] = (addr & 0xFF);
  h[2] = control;

  spi_frame_t my_frame = {.header = h,
                          .header_len = 3,
                          .tx_data = frame->tx_data,
                          .rx_data = frame->rx_data,
                          .is_read = frame->is_read,
                          .data_len = frame->data_len};

  spi_return_t spi_ret = spi1_transfer_frame(&my_frame);
  switch (spi_ret) {
  case SPI_ERR_ARG:
    return W5500_ERR_ARG;
  case SPI_ERR_TIMEOUT:
    return W5500_ERR_TRANSIENT;
  case SPI_ERR_FAULT:
    return W5500_ERR_FAULT;
  case SPI_OK:
    return W5500_OK;
  }
  return W5500_ERR_FAULT;
}

static w5500_status_t w5500_read8(uint16_t addr, uint8_t bsb, uint8_t *data) {
  spi_frame_t frame = {.is_read = true, .rx_data = data, .data_len = 1};
  return w5500_transfer(addr, bsb, &frame);
}

static w5500_status_t w5500_write8(uint16_t addr, uint8_t bsb, uint8_t v) {
  spi_frame_t frame = {.is_read = false, .tx_data = &v, .data_len = 1};
  return w5500_transfer(addr, bsb, &frame);
}

static w5500_status_t w5500_read16(uint16_t addr, uint8_t bsb, uint16_t *data) {
  uint8_t v[2] = {0, 0};
  spi_frame_t frame = {.is_read = true, .rx_data = v, .data_len = 2};
  w5500_status_t ret = w5500_transfer(addr, bsb, &frame);
  if (ret == W5500_OK)
    *data = ((uint16_t)v[0] << 8) | v[1];
  return ret;
}

/*
 * Reads a 16bit value from the same address twice to confirm its stable write.
 * If it cannot read the same value after 255 iterations returns
 * W5500_ERR_UNSTABLE. Returns status and places read data into *data.
 */
static w5500_status_t w5500_read16_stable(uint16_t addr, uint8_t bsb,
                                          uint16_t *data) {
  uint16_t a, b;
  uint8_t iterations = 0;
  w5500_status_t ret = W5500_OK;
  do {
    ret = w5500_read16(addr, bsb, &a);
    if (ret != W5500_OK)
      return ret;
    ret = w5500_read16(addr, bsb, &b);
    if (ret != W5500_OK)
      return ret;
  } while (a != b && ++iterations < 255);
  if (iterations == 255)
    return W5500_ERR_UNSTABLE;
  *data = a;
  return ret;
}

static w5500_status_t w5500_write16(uint16_t addr, uint8_t bsb, uint16_t data) {
  uint8_t v[2];
  v[0] = (uint8_t)(data >> 8);
  v[1] = (uint8_t)(data & 0x00FF);
  spi_frame_t frame = {.is_read = false, .tx_data = v, .data_len = 2};
  return w5500_transfer(addr, bsb, &frame);
}

static void w5500_reset_pin_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  SET_GPIO_MODE(GPIOA, GPIO_MODE_OUTPUT, W5500_RESET_PIN);
  SET_GPIO_OTYPER(GPIOA, 0, W5500_RESET_PIN);
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, W5500_RESET_PIN);
  GPIO_PIN_HIGH(GPIOA, W5500_RESET_PIN); // idle high
}

/*
 * Resets w5500 and waits the required amount of time.
 * Must be called from a task context beacuse of sys_sleep.
 */
static void w5500_hw_reset(void) {
  GPIO_PIN_LOW(GPIOA, W5500_RESET_PIN);
  sys_sleep(1);
  GPIO_PIN_HIGH(GPIOA, W5500_RESET_PIN);
  sys_sleep(60);
}

#define INIT_ATTEMPTS 5

/*
 * Tries to bring up the chip. Retries a number of times before giving up.
 * Returns W5500_ERR_TRANSIENT if all the reads of version hanged.
 */
static w5500_status_t w5500_bring_up(void) {
  uint8_t attempts = 0;
  uint8_t version = 0;
  w5500_status_t ret_status = W5500_OK;
  do {
    w5500_reset_pin_init();
    w5500_hw_reset();
    ret_status = w5500_get_version(&version);
    // only accepting a working read or maybe a timeout
    kprintf("attempts %d\r\n", attempts);
    kprintf("ret_status_bring_up %d\r\n", ret_status);
    kprintf("version %d\r\n", version);
    if (ret_status != W5500_OK && ret_status != W5500_ERR_TRANSIENT)
      return ret_status;
  } while (version != 0x04 && ++attempts < INIT_ATTEMPTS);

  // chip doesn't answer
  if (attempts == INIT_ATTEMPTS) {
    return W5500_ERR_STATE;
  }

  return W5500_OK;
}

/*
 * Tries to bring up the chip and if successfull configures the MAC, IP
 * GATEWAY and SUBNET.
 * Returns W5500_OK on success and any of the errors otherwise.
 */
w5500_status_t w5500_init(const uint8_t mac[6], const uint8_t ip[4],
                          const uint8_t gateway[4], const uint8_t subnet[4]) {

  w5500_status_t ret_status = W5500_OK;
  ret_status = w5500_bring_up();
  if (ret_status != W5500_OK)
    return ret_status;

  // transfer the configuration and return on any error
  spi_frame_t mac_frame = {.is_read = false, .tx_data = mac, .data_len = 6};
  spi_frame_t ip_frame = {.is_read = false, .tx_data = ip, .data_len = 4};
  spi_frame_t gateway_frame = {
      .is_read = false, .tx_data = gateway, .data_len = 4};
  spi_frame_t subnet_frame = {
      .is_read = false, .tx_data = subnet, .data_len = 4};
  ret_status = w5500_transfer(W5500_SHAR, W5500_BSB_COMMON, &mac_frame);
  if (ret_status != W5500_OK)
    return ret_status;
  ret_status = w5500_transfer(W5500_SIPR, W5500_BSB_COMMON, &ip_frame);
  if (ret_status != W5500_OK)
    return ret_status;
  ret_status = w5500_transfer(W5500_GAR, W5500_BSB_COMMON, &gateway_frame);
  if (ret_status != W5500_OK)
    return ret_status;
  ret_status = w5500_transfer(W5500_SUBR, W5500_BSB_COMMON, &subnet_frame);
  if (ret_status != W5500_OK)
    return ret_status;

  return W5500_OK;
}

w5500_status_t w5500_get_version(uint8_t *data) {
  return w5500_read8(W5500_VERSIONR, W5500_BSB_COMMON, data);
}

/*
 * Reads PHYCFGR register and sets *up on successfull read.
 * Returns the status of the read.
 */
w5500_status_t w5500_link_up(int *up) {
  if (up == 0)
    return W5500_ERR_ARG;
  uint8_t data = 0;
  w5500_status_t ret_status =
      w5500_read8(W5500_PHYCFGR, W5500_BSB_COMMON, &data);
  if (ret_status == W5500_OK)
    *up = (data & (0x01));

  return ret_status;
}

#define OPEN_MAX_TRIES 5
#define CLOSE_MAX_TRIES 5

w5500_status_t w5500_socket_close(socket_t *socket) {
  if (socket == 0 || socket->number > 7)
    return W5500_ERR_ARG;
  uint8_t tries = 0;

  w5500_status_t ret_status = w5500_write8(
      SOCKET_CR, W5500_BSB_SOCK_REG(socket->number), SOCKET_CMD_CLOSE);

  uint8_t current_sr = 1;
  do {
    sys_sleep(1);
    ret_status =
        w5500_read8(SOCKET_SR, W5500_BSB_SOCK_REG(socket->number), &current_sr);
    if (ret_status != W5500_OK && ret_status != W5500_ERR_TRANSIENT)
      return ret_status;
  } while (current_sr != SOCK_CLOSED && ++tries < CLOSE_MAX_TRIES);

  if (tries == CLOSE_MAX_TRIES) {
    socket->status = SOCKET_STATUS_ERROR;
    return W5500_ERR_TRANSIENT;
  }

  socket->status = SOCKET_STATUS_CLOSED;
  return W5500_OK;
}

w5500_status_t w5500_socket_open(socket_t *socket) {
  if (socket == 0 || socket->number > 7) {
    return W5500_ERR_ARG;
  }

  // defensive close
  w5500_status_t ret_status = w5500_socket_close(socket);
  if (ret_status != W5500_OK)
    return ret_status;

  uint8_t mode = 0;
  mode |= socket->protocol;
  ret_status =
      w5500_write8(SOCKET_MR, W5500_BSB_SOCK_REG(socket->number), mode);
  if (ret_status != W5500_OK)
    return ret_status;
  ret_status = w5500_write16(SOCKET_PORT, W5500_BSB_SOCK_REG(socket->number),
                             socket->port);
  if (ret_status != W5500_OK)
    return ret_status;

  ret_status = w5500_write8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number),
                            SOCKET_CMD_OPEN);
  if (ret_status != W5500_OK)
    return ret_status;

  uint8_t wanted_sr = 0;
  if (socket->protocol == TCP) {
    wanted_sr = SOCKET_SR_INIT;
  } else if (socket->protocol == UDP) {
    wanted_sr = SOCKET_SR_UDP;
  } else {
    socket->status = SOCKET_STATUS_ERROR;
    return W5500_ERR_ARG;
  }

  uint8_t current_sr = 0;
  uint8_t tries = 0;
  do {
    sys_sleep(2);
    ret_status =
        w5500_read8(SOCKET_SR, W5500_BSB_SOCK_REG(socket->number), &current_sr);
    if (ret_status != W5500_OK && ret_status != W5500_ERR_TRANSIENT)
      return ret_status;
  } while (wanted_sr != current_sr && ++tries < OPEN_MAX_TRIES);

  if (tries == OPEN_MAX_TRIES) {
    socket->status = SOCKET_STATUS_ERROR;
    return W5500_ERR_TRANSIENT;
  }

  uint8_t current_mr = 0;
  uint16_t current_port = 0;
  ret_status =
      w5500_read8(SOCKET_MR, W5500_BSB_SOCK_REG(socket->number), &current_mr);
  if (ret_status != W5500_OK) {
    socket->status = SOCKET_STATUS_ERROR;
    return ret_status;
  }

  ret_status = w5500_read16(SOCKET_PORT, W5500_BSB_SOCK_REG(socket->number),
                            &current_port);
  if (ret_status != W5500_OK) {
    socket->status = SOCKET_STATUS_ERROR;
    return ret_status;
  }

  if (current_mr != mode) {
    socket->status = SOCKET_STATUS_ERROR;
    return W5500_ERR_STATE;
  }
  if (current_port != socket->port) {
    socket->status = SOCKET_STATUS_ERROR;
    return W5500_ERR_STATE;
  }

  socket->status = SOCKET_STATUS_OPEN;

  return W5500_OK;
}

typedef struct {
  uint8_t src_ip[4];
  uint16_t src_port;
  uint16_t length;
} udp_header_t;

static w5500_status_t w5500_recv_udp_header(socket_t *socket,
                                            udp_header_t *header,
                                            uint16_t *read_pointer) {
  w5500_status_t ret_status = w5500_read16(
      SOCKET_RX_RD, W5500_BSB_SOCK_REG(socket->number), read_pointer);
  if (ret_status != W5500_OK)
    return ret_status;

  uint8_t buffer[8];
  spi_frame_t frame = {.rx_data = buffer, .data_len = 8, .is_read = true};
  ret_status = w5500_transfer(*read_pointer, _RX(socket->number), &frame);
  if (ret_status != W5500_OK)
    return ret_status;

  header->src_ip[0] = buffer[0];
  header->src_ip[1] = buffer[1];
  header->src_ip[2] = buffer[2];
  header->src_ip[3] = buffer[3];
  header->src_port = (buffer[4] << 8) | buffer[5];
  header->length = (buffer[6] << 8) | buffer[7];

  return W5500_OK;
}

// recevies data from the socket of length bounded by buf_len
// returns the length of data left over or -1 if there is nothing to read
w5500_status_t w5500_socket_recv(socket_t *socket, uint8_t *buffer,
                                 uint16_t maxlen, uint16_t *got,
                                 uint8_t src_ip[4], uint16_t *src_port,
                                 bool *truncated) {
  if (socket == 0 || buffer == 0 || maxlen == 0 || got == 0 || src_ip == 0 ||
      src_port == 0 || truncated == 0)
    return W5500_ERR_ARG;
  uint16_t recv_len = 0;
  w5500_status_t ret_status = w5500_read16_stable(
      SOCKET_RX_RSR, W5500_BSB_SOCK_REG(socket->number), &recv_len);
  if (ret_status != W5500_OK)
    return ret_status;

  if (recv_len == 0) {
    *got = 0;
    *truncated = false;
    return W5500_OK;
  }
  udp_header_t header = {0, 0, 0};
  uint16_t read_pointer = 0;
  ret_status = w5500_recv_udp_header(socket, &header, &read_pointer);
  if (ret_status != W5500_OK) {
    *got = 0;
    *truncated = false;
    return ret_status;
  }
  read_pointer += 8; // because of the header

  uint16_t length_to_read = header.length < maxlen ? header.length : maxlen;

  // read max possible from spi
  spi_frame_t frame = {
      .rx_data = buffer, .data_len = length_to_read, .is_read = true};
  ret_status = w5500_transfer(read_pointer, _RX(socket->number), &frame);
  if (ret_status != W5500_OK) {
    *got = 0;
    *truncated = false;
    return ret_status;
  }
  *got = length_to_read;
  for (int i = 0; i < 4; i++) {
    *(src_ip + i) = header.src_ip[i];
  }
  *src_port = header.src_port;

  // write new RX_RD which is old_RX_RD += len of header + len of data
  // regardless of if we read the data fully
  ret_status = w5500_write16(SOCKET_RX_RD, W5500_BSB_SOCK_REG(socket->number),
                             read_pointer + header.length);
  if (ret_status != W5500_OK) {
    *got = 0;
    *truncated = false;
    return ret_status;
  }
  ret_status = w5500_write8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number),
                            SOCKET_CMD_RECV);
  if (ret_status != W5500_OK) {
    // restore RX_RD to the old value because the transaction didn't commit
    w5500_write16(SOCKET_RX_RD, W5500_BSB_SOCK_REG(socket->number),
                  read_pointer - 8);
    *got = 0;
    *truncated = false;
    return ret_status;
  }
  uint8_t current_cr = SOCKET_CMD_RECV;
  uint8_t tries = 0;
  do {
    sys_sleep(1);
    ret_status =
        w5500_read8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number), &current_cr);
    if (ret_status != W5500_OK && ret_status != W5500_ERR_TRANSIENT) {
      w5500_write16(SOCKET_RX_RD, W5500_BSB_SOCK_REG(socket->number),
                    read_pointer - 8);
      *got = 0;
      *truncated = false;
      return ret_status;
    }
  } while (current_cr != 0 && ++tries < 5);
  if (tries == 5) {
    w5500_write16(SOCKET_RX_RD, W5500_BSB_SOCK_REG(socket->number),
                  read_pointer - 8);
    *got = 0;
    *truncated = false;
    return W5500_ERR_TRANSIENT;
  }
  uint16_t new_recv_len = 0xFFFF;
  ret_status = w5500_read16_stable(
      SOCKET_RX_RSR, W5500_BSB_SOCK_REG(socket->number), &new_recv_len);
  if (ret_status != W5500_OK) {
    *got = 0;
    *truncated = false;
    return ret_status;
  }
  if (new_recv_len >= recv_len) {
    *got = 0;
    *truncated = false;
    return W5500_ERR_UNSTABLE;
  }

  *truncated = header.length > maxlen;
  return W5500_OK;
}

#define SEND_TRIES 5

inline static w5500_status_t w5500_ir_clear(socket_t *socket, uint8_t mask) {
  return w5500_write8(SOCKET_IR, W5500_BSB_SOCK_REG(socket->number), mask);
}

static w5500_status_t w5500_reset_tx_wr(socket_t *socket) {
  uint16_t tx_rd = 0;
  w5500_status_t ret_status = W5500_OK;
  ret_status =
      w5500_read16(SOCKET_TX_RD, W5500_BSB_SOCK_REG(socket->number), &tx_rd);
  if (ret_status != W5500_OK)
    return ret_status;
  ret_status =
      w5500_write16(SOCKET_TX_WR, W5500_BSB_SOCK_REG(socket->number), tx_rd);
  return ret_status;
}

w5500_status_t w5500_socket_send(socket_t *socket, uint8_t *tx_data,
                                 uint16_t tx_length, uint16_t *sent,
                                 uint8_t dst_ip[4], uint16_t dst_port,
                                 uint8_t dst_mac[6]) {
  if (socket == 0 || tx_data == 0 || tx_length == 0 || dst_ip == 0 ||
      dst_port == 0 || sent == 0)
    return W5500_ERR_ARG;

  uint16_t tx_available = 0;
  uint8_t tries = 0;
  w5500_status_t ret_status = W5500_OK;

  if (socket->has_unreaped_send) {
    uint8_t ir = 0;
    ret_status =
        w5500_read8(SOCKET_IR, W5500_BSB_SOCK_REG(socket->number), &ir);
    if (ret_status != W5500_OK)
      return ret_status;
    if (ir & SOCKET_IR_SENDOK) {

      socket->has_unreaped_send = false;
      ret_status = w5500_ir_clear(socket, SOCKET_IR_SENDOK);
      if (ret_status != W5500_OK)
        return ret_status;

    } else if (ir & SOCKET_IR_TIMEOUT) {
      ret_status = w5500_ir_clear(socket, SOCKET_IR_TIMEOUT);
      if (ret_status != W5500_OK)
        return ret_status;

      ret_status = w5500_reset_tx_wr(socket);
      if (ret_status != W5500_OK)
        return ret_status;

      socket->has_unreaped_send = false;
    } else {
      *sent = 0;
      return W5500_ERR_TRANSIENT;
    }
  } else {
    ret_status = w5500_ir_clear(socket, SOCKET_IR_SENDOK | SOCKET_IR_TIMEOUT);
    if (ret_status != W5500_OK)
      return ret_status;
  }

  do {
    sys_sleep(1);
    ret_status = w5500_read16_stable(
        SOCKET_TX_FSR, W5500_BSB_SOCK_REG(socket->number), &tx_available);
    if (ret_status != W5500_OK && ret_status != W5500_ERR_TRANSIENT)
      return ret_status;
  } while (tx_available < tx_length && ++tries < SEND_TRIES);

  if (tries == SEND_TRIES) {
    *sent = 0;
    return W5500_ERR_TRANSIENT;
  }

  uint16_t write_pointer = 0;
  ret_status = w5500_read16(SOCKET_TX_WR, W5500_BSB_SOCK_REG(socket->number),
                            &write_pointer);
  if (ret_status != W5500_OK) {
    *sent = 0;
    return ret_status;
  }

  spi_frame_t frame = {
      .tx_data = tx_data, .data_len = tx_length, .is_read = false};
  ret_status = w5500_transfer(write_pointer, _TX(socket->number), &frame);
  if (ret_status != W5500_OK) {
    *sent = 0;
    return ret_status;
  }

  frame.tx_data = dst_ip;
  frame.data_len = 4;
  frame.is_read = false;

  ret_status =
      w5500_transfer(SOCKET_DIPR, W5500_BSB_SOCK_REG(socket->number), &frame);
  if (ret_status != W5500_OK) {
    *sent = 0;
    return ret_status;
  }
  ret_status =
      w5500_write16(SOCKET_DPORT, W5500_BSB_SOCK_REG(socket->number), dst_port);
  if (ret_status != W5500_OK) {
    *sent = 0;
    return ret_status;
  }
  if (dst_mac != 0) {
    frame.tx_data = dst_mac;
    frame.data_len = 4;
    frame.is_read = false;

    ret_status =
        w5500_transfer(SOCKET_DHAR, W5500_BSB_SOCK_REG(socket->number), &frame);
    if (ret_status != W5500_OK) {
      *sent = 0;
      return ret_status;
    }
  }

  ret_status = w5500_write16(SOCKET_TX_WR, W5500_BSB_SOCK_REG(socket->number),
                             write_pointer + tx_length);
  if (ret_status != W5500_OK) {
    *sent = 0;
    return ret_status;
  }

  w5500_ir_clear(socket, SOCKET_IR_SENDOK);

  uint8_t cmd_to_send = dst_mac == 0 ? SOCKET_CMD_SEND : SOCKET_CMD_SEND_MAC;

  ret_status =
      w5500_write8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number), cmd_to_send);
  if (ret_status != W5500_OK) {
    w5500_write16(SOCKET_TX_WR, W5500_BSB_SOCK_REG(socket->number),
                  write_pointer);
    *sent = 0;
    return ret_status;
  }

  uint8_t current_cr = SOCKET_CMD_SEND;
  tries = 0;
  do {
    sys_sleep(1);
    ret_status =
        w5500_read8(SOCKET_CR, W5500_BSB_SOCK_REG(socket->number), &current_cr);
    if (ret_status != W5500_OK && ret_status != W5500_ERR_TRANSIENT) {
      w5500_write16(SOCKET_TX_WR, W5500_BSB_SOCK_REG(socket->number),
                    write_pointer);
      *sent = 0;
      return ret_status;
    }
  } while (current_cr != 0 && ++tries < 5);
  if (tries == 5) {
    w5500_write16(SOCKET_TX_WR, W5500_BSB_SOCK_REG(socket->number),
                  write_pointer);
    *sent = 0;
    return W5500_ERR_TRANSIENT;
  }

  uint8_t current_ir = 0;
  tries = 0;
  do {
    sys_sleep(1);
    ret_status =
        w5500_read8(SOCKET_IR, W5500_BSB_SOCK_REG(socket->number), &current_ir);
    if (ret_status != W5500_OK && ret_status != W5500_ERR_TRANSIENT) {
      w5500_write16(SOCKET_TX_WR, W5500_BSB_SOCK_REG(socket->number),
                    write_pointer);
      *sent = 0;
      return ret_status;
    }
    if (current_ir & SOCKET_IR_TIMEOUT) {
      w5500_reset_tx_wr(socket);
      w5500_ir_clear(socket, SOCKET_IR_TIMEOUT);
      *sent = 0;
      return W5500_ERR_UNREACHABLE;
    }
  } while (!(current_ir & SOCKET_IR_SENDOK) && ++tries < 5);

  if (tries == 5) {
    socket->has_unreaped_send = true;
  }

  *sent = tx_length;
  return W5500_OK;
}
