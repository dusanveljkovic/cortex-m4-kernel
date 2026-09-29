# Debug CLI

**File:** `source/kernel/cli.c`, `include/cli.h`

A minimal interactive shell over the console, running as its own task
(`cli_task`, created and queued in `main.c`). See the [README](../README.md)
for the commands it exposes and what output to expect. This task runs as 
privileged because it needs to read kernel internal state. 

## How input is handled

`cli_task` loops calling `console_read()` one character at a time (blocking,
see [USART2 Driver](09-usart-driver.md)) and feeds each character to
`cli_input_char()`:

- **Enter** (`\r` or `\n`): null-terminates the line buffer, echoes a
  newline, and calls `cli_execute()` on it.
- **Backspace** (`\b` or `127` Backspace key): if there's anything to erase, 
  removes the last character
  from the buffer and writes `\b \b` (back up, overwrite with a space, back
  up again) so the terminal's own display actually erases the character,
  not just the buffer.
- **Anything else**: appended to a 128-byte line buffer (silently dropped
  past that limit) and echoed back so the terminal shows what's being typed.

## Commands

Each command is a `{name, description, function}` entry in a static table.
`cli_cmd_help` prints every entry's description; the rest read from
`static_memory.h`'s `get_task()`/`N_TASKS`, `heap.h`'s `heap_free_size()`, or
linker-provided symbols (`_heap_start`/`_end`, `_vma_data_start`/`_end`,
`_bss_start`/`_end`) to report memory usage. `reboot` writes
`SCB->AIRCR` directly to request a system reset, with `__DSB()` barriers
before and after to make sure the write has actually taken effect before
anything else can happen.

