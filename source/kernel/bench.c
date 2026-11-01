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
    bucket = 127;
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

bench_result_t *get_preemption_result(void) { return &preemption_result; }
bench_result_t *get_pendsv_result(void) { return &pendsv_result; }
bench_result_t *get_preemption_fp_result(void) { return &preemption_fp_result; }
bench_result_t *get_pendsv_fp_result(void) { return &pendsv_fp_result; }
bench_result_t *get_irq_result(void) { return &irq_result; }
