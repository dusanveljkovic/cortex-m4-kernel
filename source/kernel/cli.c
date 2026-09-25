#include "../../include/cli.h"
#include "../../include/static_memory.h"
#include "../../include/usart.h"
#include "../../include/utils.h"
#include "stdint.h"

#define CLI_BUFFER_SIZE 128

static char cli_buffer[CLI_BUFFER_SIZE] = {0};
static uint16_t cli_pos = 0;

extern uint32_t system_ticks;

typedef void (*cli_command_fn)(int argc, char **argv);

typedef struct {
  const char *name;
  const char *description;
  cli_command_fn function;
} cli_command_t;

void cli_execute(const char *cmd);
void cli_cmd_ps(int argc, char **argv);
void cli_cmd_help(int argc, char **argv);
void cli_cmd_uptime(int argc, char **argv);
void cli_cmd_mem(int argc, char **argv);
void cli_cmd_heap(int argc, char **argv);
void cli_cmd_mutex(int argc, char **argv);
void cli_cmd_sem(int argc, char **argv);
void cli_cmd_reboot(int argc, char **argv);

static const cli_command_t commands[] = {
    {"help", "Print available commands", cli_cmd_help},
    {"ps", "Show info about running processes", cli_cmd_ps},
    {"uptime", "Print system uptime", cli_cmd_uptime},
    {"mem", "Print memory usage", cli_cmd_mem},
    {"heap", "Print heap usage", cli_cmd_heap},
    {"mutex", "Print mutex owners/waiters", cli_cmd_mutex},
    {"sem", "Print semaphore state", cli_cmd_sem},
    {"reboot", "Reboot the MCU", cli_cmd_reboot},
};

int cli_input_char(char c) {
  if (c == '\r' || c == '\n') {
    cli_buffer[cli_pos] = '\0';
    printf("\r\n");
    cli_execute(cli_buffer);
    cli_pos = 0;
    return 1;
  }

  if (c == '\b' || c == 127) {
    if (cli_pos > 0) {
      cli_pos--;
      printf("\b \b");
    }
    return 0;
  }

  if (cli_pos < CLI_BUFFER_SIZE - 1) {
    cli_buffer[cli_pos++] = c;
    usart2_putc(c);
  }
  return 0;
}

void cli_task(void *arg) {
  printf("\r\nCLI\r\n");
  printf("> ");

  while (1) {
    char c;
    usart2_getc(&c);
    if (cli_input_char(c) == 1) {
      printf("> ");
    }
  }
}

void cli_execute(const char *line) {
  if (line[0] == '\0')
    return;
  for (int i = 0; i < 8; i++) {
    const cli_command_t *cmd = &commands[i];
    if (strcmp(cmd->name, line) == 0) {
      cmd->function(0, 0);
      return;
    }
  }
  printf("unknown command %s\r\n", line);
}

void cli_cmd_ps(int argc, char **argv) {
  printf("PID\tNAME\tSTATE\tPRI EPRI\tSP\r\n");
  printf("-----------------------\r\n");

  for (int i = 0; i < N_TASKS; i++) {
    tcb_t *task = get_task(i);

    if (task->state == TASK_UNUSED)
      continue;

    const char *state;
    switch (task->state) {
    case TASK_READY:
      state = "READY";
      break;
    case TASK_RUNNING:
      state = "RUNNING";
      break;
    case TASK_BLOCKED:
      state = "BLOCKED";
      break;
    case TASK_SLEEPING:
      state = "SLEEPING";
      break;
    case TASK_FINISHED:
      state = "FINISHED";
      break;
    default:
      state = "UNKNOWN";
      break;
    }
    printf("%u\t%s\t%s\t%u %u\t%x\t%s\r\n", (unsigned)i, task->name, state,
           task->base_priority, task->effective_priority, (uint32_t)task->sp,
           task == current_task ? "*" : "");
  }
}

void cli_cmd_help(int argc, char **argv) {
  for (int i = 0; i < 8; i++) {
    const cli_command_t *cmd = &commands[i];
    printf("%s > %s\r\n", cmd->name, cmd->description);
  }
}
void cli_cmd_uptime(int argc, char **argv) {
  printf("Uptime: %u ms\r\n", system_ticks);
}

extern uint8_t _vma_data_start;
extern uint8_t _vma_data_end;
extern uint8_t _bss_start;
extern uint8_t _bss_end;
extern uint8_t _heap_start;
extern uint8_t _heap_end;

#define SRAM_START 0x20000000
#define SRAM_END 0x20020000

void cli_cmd_mem(int argc, char **argv) {
  uintptr_t data_start = (uintptr_t)&_vma_data_start;
  uintptr_t data_end = (uintptr_t)&_vma_data_end;

  uintptr_t bss_start = (uintptr_t)&_bss_start;
  uintptr_t bss_end = (uintptr_t)&_bss_end;

  uintptr_t heap_start = (uintptr_t)&_heap_start;
  uintptr_t heap_end = (uintptr_t)&_heap_end;

  uint32_t data_size = data_end - data_start;
  uint32_t bss_size = bss_end - bss_start;
  uint32_t heap_size = heap_end - heap_start;

  uint32_t static_used = data_size + bss_size;

  printf("Memory\r\n");
  printf("----------------------------\r\n");

  printf("SRAM    : %u KB\r\n", (unsigned)(SRAM_END - SRAM_START) / 1024);

  printf(".data   : %u bytes\r\n", data_size);
  printf(".bss    : %u bytes\r\n", bss_size);
  printf("static  : %u bytes\r\n", static_used);

  printf("heap    : %u KB\r\n", (heap_size / 1024));
  printf("heap @  : %x\r\n", (uint32_t)heap_start);
  printf("heap -> : %x\r\n", (uint32_t)heap_end);
}
void cli_cmd_heap(int argc, char **argv) { printf("not implemented\r\n"); }
void cli_cmd_mutex(int argc, char **argv) { printf("not implemented\r\n"); }
void cli_cmd_sem(int argc, char **argv) { printf("not implemented\r\n"); }
void cli_cmd_reboot(int argc, char **argv) { printf("not implemented\r\n"); }
