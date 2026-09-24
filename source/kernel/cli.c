#include "../../include/cli.h"
#include "../../include/usart.h"
#include "stdint.h"

#define CLI_BUFFER_SIZE 128
#define print(s) usart2_puts(s)
#define putc(c) usart2_putc(c)

static char cli_buffer[CLI_BUFFER_SIZE] = {0};
static uint16_t cli_pos = 0;

void cli_execute(const char *cmd);

void cli_input_char(char c) {
  if (c == '\r' || c == '\n') {
    cli_buffer[cli_pos] = '\0';
    cli_execute(cli_buffer);
    cli_pos = 0;
    return;
  }

  if (c == '\b' || c == 127) {
    if (cli_pos > 0) {
      cli_pos--;
      print("\b \b");
    }
    return;
  }

  if (cli_pos < CLI_BUFFER_SIZE - 1) {
    cli_buffer[cli_pos++] = c;
    putc(c);
  }
}

void cli_task(void *arg) {
  print("\r\nCLI\r\n");
  print("> ");

  while (1) {
    char c;
    usart2_getc(&c);
    cli_input_char(c);
  }
}

void cli_execute(const char *cmd) { print(cmd); }
