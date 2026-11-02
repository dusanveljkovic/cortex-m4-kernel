#ifndef _BENCH_H
#define _BENCH_H

#include <stdint.h>

extern volatile uint32_t bench_pendsv_start;
extern volatile uint32_t bench_pendsv_end;
extern volatile uint32_t bench_pendsv_lr;
extern volatile uint32_t bench_preemption_start;

extern volatile uint32_t bench_irq_t0;
extern volatile uint32_t bench_irq_site;

typedef struct {
  volatile uint32_t DHCSR;
  volatile uint32_t DCRSR;
  volatile uint32_t DCRDR;
  volatile uint32_t DEMCR;
} CoreDebug_RegisterMap;

typedef struct {
  volatile uint32_t CTRL;
  volatile uint32_t CYCCNT;
} DWT_RegisterMap;

#define CoreDebug ((volatile CoreDebug_RegisterMap *)0xE000EDF0)
#define DWT ((volatile DWT_RegisterMap *)0xE0001000)

#define CoreDebug_DEMCR_TRCENA (1u << 24)

#define DWT_CTRL_CYCCNTENA (1u << 0)

#define HIST_BUCKETS 256
typedef struct {
  uint16_t hist[HIST_BUCKETS]; // 256 buckets * 8 cycle resolution = 0 - 2047
  uint16_t overflow;
  uint32_t n, sum, min, max;
} bench_result_t;

void enable_counter(void);

void reset_counter(void);

static inline uint32_t read_cycles(void) { return DWT->CYCCNT; }

uint32_t calc_percentile(bench_result_t *result, uint32_t target);

bench_result_t bench_syscall_roundtrip(void);
void update_irq_result(uint32_t new_reading, uint32_t here);

bench_result_t *get_preemption_result(void);
bench_result_t *get_pendsv_result(void);
bench_result_t *get_preemption_fp_result(void);
bench_result_t *get_pendsv_fp_result(void);
bench_result_t *get_irq_result(void);

#endif // !_BENCH_H
