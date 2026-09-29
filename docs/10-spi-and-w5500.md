# SPI1 & W5500 Ethernet

**Files:** `source/drivers/spi.c/.h`, `source/drivers/w5500.c/.h`,
`include/gpio.h`

## SPI1

Pins, all on GPIOA (the Nucleo's Arduino header): **SCK=PA5, MISO=PA6,
MOSI=PA7, CS=PA4**. 

Chip-select is a **plain GPIO output**, not SPI1's hardware NSS pin
`spi1_init()` sets `SSM`/`SSI` in `CR1` specifically to take NSS out of the
picture, since in master mode the hardware NSS pin is prone to a spurious
`MODF` fault if anything pulls it low unexpectedly. `spi1_cs_select()`/
`spi1_cs_deselect()` drive PA4 directly (active low), which also means a
multi-byte transaction can stay under one CS assertion for as long as the
caller wants, regardless of how many `spi1_transfer` calls it takes.

`SPI1->CR1` is configured for master mode, `SCK = APB2 / 8` (84 MHz / 8 =
10.5 MHz, see [Clock & Power](02-clock-and-power.md)), and mode 0
(`CPOL=0`/`CPHA=0`, the implicit value since those bits are simply left out
of the OR'd-together `CR1` value).

`spi1_transfer(data)` is the single full-duplex primitive every SPI
transaction is built from. The peripheral is a shift register, so every
clock that pushes a bit out on MOSI simultaneously shifts one in on MISO;
there's no way to "only send" or "only receive" at the hardware level.
`spi1_write`/`spi1_read`/`spi1_transfer_buffer` are all just loops around it.

## W5500

The W5500 is an SPI-attached hardware TCP/IP offload chip. Every transaction
uses a fixed 3-byte frame header which consists of 16-bit register address, then a control
byte encoding which internal "block" you're addressing and the read/write
direction, followed by a data phase, all under one CS assertion:

```c
static void w5500_write(uint16_t addr, uint8_t bsb, const uint8_t *data, uint16_t len) {
  uint8_t control = (bsb << 3) | W5500_CTRL_RWB_WRITE | W5500_CTRL_OM_VDM;
  spi1_cs_select();
  spi1_transfer((addr >> 8) & 0xFF);
  spi1_transfer(addr & 0xFF);
  spi1_transfer(control);
  spi1_write(data, len);
  spi1_cs_deselect();
}
```

### Current scope

As it stands, this driver covers **chip bring-up and two diagnostics only**:

- `w5500_init(mac, ip, gateway, subnet)` - resets the chip (see below) and
  writes the MAC/IP/gateway/subnet mask into the common register block.
- `w5500_get_version()` - reads `VERSIONR`, which should always read back
  `0x04` on a genuine W5500.
- `w5500_link_up()` - reads the `LNK` bit out of `PHYCFGR`.

There is currently **no socket-level API** 

### Reset sequence, and where it can be called from

```c
static void w5500_hw_reset(void) {
  GPIO_PIN_LOW(GPIOA, W5500_RESET_PIN);
  sys_sleep(1);
  GPIO_PIN_HIGH(GPIOA, W5500_RESET_PIN);
  sys_sleep(50);
}
```

The reset pin is held low briefly, then released, then the code waits for
the chip's internal PLL to lock (datasheet: up to 50 ms) before any register
access will get a valid response. This now uses the real tick-based
`sys_sleep()` syscall (see [Syscalls](04-syscalls.md)) rather than a
busy-wait loop - which means **`w5500_init()` must be called from inside a
task, after the scheduler is running, not from `main()` before
`systick_init()`.** `sleep_for_ticks()` silently no-ops if there's no current
task yet (`current_task == 0`), so calling this before the scheduler starts
would skip the delay entirely without any error - the chip likely wouldn't
be ready yet when the subsequent register writes happen.
