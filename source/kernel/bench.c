#include "../../include/kernel/bench.h"
#include "../../include/kernel/syscall.h"
#include "../../include/kernel/utils.h"
#include <stdbool.h>

volatile uint32_t bench_pendsv_start;
volatile uint32_t bench_pendsv_end;
volatile uint32_t bench_pendsv_lr;
volatile uint32_t bench_preemption_start;

volatile uint32_t bench_irq_t0;
volatile uint32_t bench_irq_site;

static bench_result_t preemption_result = {.min = 0xFFFFFFFF};
static bench_result_t preemption_fp_result = {.min = 0xFFFFFFFF};
static bench_result_t pendsv_result = {.min = 0xFFFFFFFF};
static bench_result_t pendsv_fp_result = {.min = 0xFFFFFFFF};
static bench_result_t irq_result = {.min = 0xFFFFFFFF};

void enable_counter(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA;
  reset_counter();
}

void reset_counter(void) {
  DWT->CYCCNT = 0;

  DWT->CTRL |= DWT_CTRL_CYCCNTENA;
}

static inline void update_result(bench_result_t *result, uint32_t new_reading) {
  uint16_t bucket = new_reading >> 3; // 8 cycle resolution
  if (bucket >= HIST_BUCKETS) {
    result->overflow++;
    bucket = HIST_BUCKETS - 1;
  }
  result->hist[bucket]++;

  result->n++;
  result->sum += new_reading;
  if (new_reading > result->max)
    result->max = new_reading;
  if (new_reading < result->min)
    result->min = new_reading;
}
void update_irq_result(uint32_t new_reading, uint32_t here) {
  if (new_reading > irq_result.max) {
    bench_irq_site = here;
  }
  update_result(&irq_result, new_reading);
}

uint32_t bench_record_switch(uint32_t exc_return) {
  uint32_t preemption_delta = bench_pendsv_end - bench_preemption_start;
  uint32_t pendsv_delta = bench_pendsv_end - bench_pendsv_start;
  bool fpu = !(exc_return & 0x10) || !(bench_pendsv_lr & 0x10);
  if (!fpu) {
    update_result(&preemption_result, preemption_delta);
    update_result(&pendsv_result, pendsv_delta);
  } else {
    update_result(&preemption_fp_result, preemption_delta);
    update_result(&pendsv_fp_result, pendsv_delta);
  }
  return exc_return;
}

uint32_t calc_percentile(bench_result_t *result, uint32_t target) {
  uint32_t cum = 0;
  uint32_t val = 0;
  for (uint16_t b = 0; b < HIST_BUCKETS; b++) {
    if (cum + result->hist[b] > target) {
      val = (b << 3) + (8 * (target - cum)) / result->hist[b];
      break;
    }
    cum += result->hist[b];
  }
  if (val < result->min)
    val = result->min;
  if (val > result->max)
    val = result->max;
  return val;
}

#define SYSCALL_ROUNDTRIP_ITER 1000
bench_result_t bench_syscall_roundtrip(void) {
  bench_result_t res;
  memset(&res, 0, sizeof(bench_result_t));
  res.min = 0xFFFFFFFF;
  for (int i = 0; i < SYSCALL_ROUNDTRIP_ITER; i++) {
    volatile uint32_t start = read_cycles();
    syscall(SYS_UPTIME, 0, 0, 0, 0);
    volatile uint32_t delta = read_cycles() - start;
    update_result(&res, delta);
  }
  return res;
}

void bench_reset(void) {
  memset(&preemption_result, 0, sizeof(bench_result_t));
  preemption_result.min = 0xFFFFFFFF;

  memset(&preemption_fp_result, 0, sizeof(bench_result_t));
  preemption_fp_result.min = 0xFFFFFFFF;

  memset(&pendsv_result, 0, sizeof(bench_result_t));
  pendsv_result.min = 0xFFFFFFFF;

  memset(&pendsv_fp_result, 0, sizeof(bench_result_t));
  pendsv_fp_result.min = 0xFFFFFFFF;

  memset(&irq_result, 0, sizeof(bench_result_t));
  irq_result.min = 0xFFFFFFFF;

  bench_stop_flag = false;
}
typedef struct {
  semaphore_t *sem1;
  semaphore_t *sem2;
  semaphore_t *sem3;
} arg_wrap_t;

static void ping_task(void *arg);
static void pong_task(void *arg);
static void fpu_task(void *arg);
volatile bool bench_stop_flag = false;

void start_workload(void) {
  semaphore_t *sem1 = sys_sem_create(1);
  semaphore_t *sem2 = sys_sem_create(0);
  semaphore_t *sem3 = sys_sem_create(0);
  arg_wrap_t *arg1 = (arg_wrap_t *)sys_malloc(sizeof(arg_wrap_t));
  arg1->sem1 = sem1;
  arg1->sem2 = sem2;
  arg1->sem3 = sem3;
  sys_task_create(3, ping_task, arg1);
  sys_task_create(3, pong_task, arg1);
  sys_task_create(3, fpu_task, arg1);
}

bench_result_t *get_preemption_result(void) { return &preemption_result; }
bench_result_t *get_pendsv_result(void) { return &pendsv_result; }
bench_result_t *get_preemption_fp_result(void) { return &preemption_fp_result; }
bench_result_t *get_pendsv_fp_result(void) { return &pendsv_fp_result; }
bench_result_t *get_irq_result(void) { return &irq_result; }

static void ping_task(void *arg) {
  arg_wrap_t *a = (arg_wrap_t *)arg;
  while (!bench_stop_flag) {
    sys_sem_wait(a->sem1);
    sys_sem_post(a->sem2);
  }
}

static void pong_task(void *arg) {
  arg_wrap_t *a = (arg_wrap_t *)arg;
  while (!bench_stop_flag) {
    sys_sem_wait(a->sem2);
    sys_sem_post(a->sem3);
  }
}

static void fpu_task(void *arg) {
  arg_wrap_t *a = (arg_wrap_t *)arg;
  float x = 1.0, y = 3.2;
  volatile float z = 0.0;
  while (!bench_stop_flag) {
    sys_sem_wait(a->sem3);
    sys_sem_post(a->sem1);
    z += x * y;
  }
}
