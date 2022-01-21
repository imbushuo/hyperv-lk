// Hyper-V GICv3 implementation

/*
 * Copyright (c) 2012-2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <assert.h>
#include <sys/types.h>

#include <lk/bits.h>
#include <lk/debug.h>
#include <lk/err.h>
#include <sys/types.h>

#include <gicv3.h>

#include <lk/reg.h>

#include <kernel/debug.h>
#include <kernel/thread.h>

#include <lk/init.h>

#include <platform/interrupts.h>

#include <arch/ops.h>

#include <platform/gic.h>

#include <lk/trace.h>
#if WITH_LIB_SM
#error "Unsupported"
#endif

#if ARCH_ARM64
#include <arch/arm64.h>
#define iframe arm64_iframe_short
#define IFRAME_PC(frame) ((frame)->elr)
#endif

#define GIC_MAX_PER_CPU_INT 32

static bool arm_gic_interrupt_change_allowed(int irq) { return true; }
static void suspend_resume_fiq(bool resume_gicc, bool resume_gicd) {}

struct int_handler_struct {
  int_handler handler;
  void *arg;
};

static struct int_handler_struct int_handler_table_per_cpu[GIC_MAX_PER_CPU_INT]
                                                          [SMP_MAX_CPUS];
static struct int_handler_struct
    int_handler_table_shared[MAX_INT - GIC_MAX_PER_CPU_INT];

static struct int_handler_struct *get_int_handler(unsigned int vector,
                                                  uint cpu) {
  if (vector < GIC_MAX_PER_CPU_INT)
    return &int_handler_table_per_cpu[vector][cpu];
  else
    return &int_handler_table_shared[vector - GIC_MAX_PER_CPU_INT];
}

void register_int_handler(unsigned int vector, int_handler handler, void *arg) {
}

void arm_gic_init(void) {}

status_t mask_interrupt(unsigned int vector) { return NO_ERROR; }

status_t unmask_interrupt(unsigned int vector) { return NO_ERROR; }

static enum handler_return __platform_irq(struct iframe *frame) {
  return INT_NO_RESCHEDULE;
}

enum handler_return platform_irq(struct iframe *frame);
enum handler_return platform_irq(struct iframe *frame) {
  return __platform_irq(frame);
}

void platform_fiq(struct iframe *frame);
void platform_fiq(struct iframe *frame) { PANIC_UNIMPLEMENTED; }
