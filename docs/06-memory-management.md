# Memory Management

**Files:** `source/kernel/heap.c`, `include/heap.h`,
`source/kernel/static_memory.c`, `include/static_memory.h`

There are two entirely separate memory pools, backing different kinds of
allocation:

## Static pools: tasks and their stacks

```c
#define N_TASKS 16
#define TASK_STACK_SIZE 1024

static tcb_t task_slots[N_TASKS];
__attribute__((aligned(TASK_STACK_SIZE)))
static uint8_t task_stacks[N_TASKS][TASK_STACK_SIZE];
```

Fixed arrays, no dynamic sizing. `alloc_task()` scans for a slot that's
`TASK_UNUSED` or `TASK_FINISHED` and claims it, setting `stack_base`/
`stack_size` to point at that slot's row of `task_stacks`. The whole scan
runs inside `irq_save()`/`irq_restore()` so two overlapping allocation
attempts can't both claim the same slot.

`task_stacks` is deliberately aligned to `TASK_STACK_SIZE` - the MPU requires
a region's base address to be naturally aligned to its size (see
[Memory Protection (MPU)](07-memory-protection-mpu.md)), so this alignment is
what makes each task's stack a valid, individually-protectable MPU region.

## The heap

A classic **first-fit allocator with immediate coalescing**, over a single
linked list of variable-sized blocks:

```c
typedef struct heap_block {
  uint32_t size;
  uint8_t free;
  struct heap_block *next;
  struct heap_block *prev;
} heap_block_t;
```

`heap_init(start, size)` carves the entire region (defined in
`linker_script.ld`: `0x20010000`, 64 KB) into one large free block.

- **`kmalloc(size)`**: walks the list for the first free block big enough.
  If the block is comfortably larger than what's needed (enough left over
  for another header plus the minimum alignment), it's **split** 
- **`kfree(ptr)`**: marks the block free, then coalesces with the next block
  if it's also free, and separately with the previous block if *it's* also
  free 
- **`kcalloc(count, size)`**: calls `kmalloc` and zeroes the result.

Every entry point runs inside `irq_save()`/`irq_restore()`.
