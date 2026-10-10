/* ==================================================================
 * FPU context test — register-level, compiler-independent
 * ==================================================================
 * Inline asm is used deliberately: at -O0 GCC spills nearly everything,
 * so a register leak can leave arithmetic correct. asm is opaque at
 * every optimisation level and names exactly the registers under test.
 *
 * Word patterns are arbitrary bits. vldm/vstm are RAW transfers, so NaN
 * and denormal patterns round-trip unchanged and no FP arithmetic is
 * involved. FPSCR's NZCV is stamped separately.
 * ================================================================== */

#include "../../include/semaphore.h"
#define FPU_NWORDS 16
#include "../../include/syscall.h"
#include "uprintf.h"
#include <stdint.h>

static inline void fpu_ld_lo(const uint32_t *p) {
  __asm volatile("vldmia %0, {s0-s15}" ::"r"(p) : "memory");
}
static inline void fpu_st_lo(uint32_t *p) {
  __asm volatile("vstmia %0, {s0-s15}" ::"r"(p) : "memory");
}
static inline void fpu_ld_hi(const uint32_t *p) {
  __asm volatile("vldmia %0, {s16-s31}" ::"r"(p) : "memory");
}
static inline void fpu_st_hi(uint32_t *p) {
  __asm volatile("vstmia %0, {s16-s31}" ::"r"(p) : "memory");
}
static inline uint32_t fpscr_get(void) {
  uint32_t v;
  __asm volatile("vmrs %0, fpscr" : "=r"(v));
  return v;
}
/* FPSCR[31:28] are the writable condition flags — harmless, but genuine
   per-context state that the context switch is obliged to carry over. */
static inline void fpscr_set_nzcv(uint32_t bits) {
  uint32_t v = fpscr_get();
  v = (v & ~0xF0000000u) | (bits & 0xF0000000u);
  __asm volatile("vmsr fpscr, %0" ::"r"(v));
}

typedef struct {
  const char *name;
  uint32_t lo_base; /* S0-S15  signature */
  uint32_t hi_base; /* S16-S31 signature */
  uint32_t nzcv;    /* FPSCR[31:28] signature */
  semaphore_t *my_mutex;
  semaphore_t *other_mutex;
} fpu_sig_t;

static void sig_fill(uint32_t *buf, uint32_t base) {
  for (int i = 0; i < FPU_NWORDS; i++)
    buf[i] = base + (uint32_t)i;
}

void fpu_test_task(void *arg) {
  const fpu_sig_t *sig = (const fpu_sig_t *)arg;

  uint32_t want_lo[FPU_NWORDS], want_hi[FPU_NWORDS];
  uint32_t got_lo[FPU_NWORDS], got_hi[FPU_NWORDS];
  sig_fill(want_lo, sig->lo_base);
  sig_fill(want_hi, sig->hi_base);

  uint32_t rounds = 0, bad_lo = 0, bad_hi = 0, bad_fp = 0;
  int printed = 0;

  while (1) {
    /* 1. stamp our identity into every piece of FPU context we own */
    fpu_ld_lo(want_lo);
    fpu_ld_hi(want_hi);
    fpscr_set_nzcv(sig->nzcv);

    /* 2. force a genuine PendSV switch. The other task runs and stamps
          ITS identity into the same physical registers. */
    sys_sem_wait(sig->other_mutex);
    sys_yield();
    // uprintf("%s", sig->name);

    /* 3. whatever comes back had better be ours */
    fpu_st_lo(got_lo);
    fpu_st_hi(got_hi);
    uint32_t got_fp = fpscr_get();

    int ok_lo = 1, ok_hi = 1;
    for (int i = 0; i < FPU_NWORDS; i++) {
      if (got_lo[i] != want_lo[i])
        ok_lo = 0;
      if (got_hi[i] != want_hi[i])
        ok_hi = 0;
    }
    int ok_fp = ((got_fp & 0xF0000000u) == (sig->nzcv & 0xF0000000u));

    rounds++;
    if (!ok_lo)
      bad_lo++;
    if (!ok_hi)
      bad_hi++;
    if (!ok_fp)
      bad_fp++;

    if ((!ok_lo || !ok_hi || !ok_fp) && printed < 5) {
      printed++;
      uprintf("[fpu %s] FAIL round %u  S0-S15:%s  S16-S31:%s  FPSCR:%s\r\n",
              sig->name, (unsigned)rounds, ok_lo ? "ok" : "BAD",
              ok_hi ? "ok" : "BAD", ok_fp ? "ok" : "BAD");
    }
    if ((rounds % 10000u) == 0u)
      uprintf("[fpu %s] %u rounds | bad lo=%u hi=%u fpscr=%u\r\n", sig->name,
              (unsigned)rounds, (unsigned)bad_lo, (unsigned)bad_hi,
              (unsigned)bad_fp);
    sys_sem_post(sig->my_mutex);
  }
}

static fpu_sig_t SIG_A = {"A", 0xA0000000u, 0xBB000000u, 0x10000000u};
static fpu_sig_t SIG_B = {"B", 0xC0000000u, 0xDD000000u, 0x00000000u};

/* in user_main(): */
void fpu_test_start(void) {
  semaphore_t *a_mutex = sys_sem_create(1);
  semaphore_t *b_mutex = sys_sem_create(0);
  SIG_A.my_mutex = a_mutex;
  SIG_A.other_mutex = b_mutex;
  SIG_B.my_mutex = b_mutex;
  SIG_B.other_mutex = a_mutex;
  tcb_t *fa = sys_task_create(1, fpu_test_task, (void *)&SIG_A);
  tcb_t *fb = sys_task_create(1, fpu_test_task, (void *)&SIG_B);
  sys_task_set_name(fa, "fpu A");
  sys_task_set_name(fb, "fpu B");
}
