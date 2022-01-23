# Assuming loading at 0000000004000000 is currently safe
# Forked from qemu-virt-arm

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

ifeq ($(ARCH),)
ARCH := arm64
endif
ifeq ($(ARCH),arm64)
ARM_CPU ?= cortex-a53
endif

# Intentionally
WITH_SMP := 0

LK_HEAP_IMPLEMENTATION ?= dlmalloc

MODULE_SRCS += \
    $(LOCAL_DIR)/debug.c \
    $(LOCAL_DIR)/platform.c \
    $(LOCAL_DIR)/uart.c \
    $(LOCAL_DIR)/psci.S

MEMBASE := 0x00000000
MEMSIZE ?= 0xE0000000   # 3584MB memory
KERNEL_LOAD_OFFSET := 0x04000000 # 64MB

MODULE_DEPS += \
    lib/cbuf \
    dev/timer/arm_generic \
    dev/interrupt/arm_gic_v3

GLOBAL_DEFINES += \
    MEMBASE=$(MEMBASE) \
    MEMSIZE=$(MEMSIZE) \
    PLATFORM_SUPPORTS_PANIC_SHELL=1 \
    CONSOLE_HAS_INPUT_BUFFER=1 \
    TIMER_ARM_GENERIC_SELECTED=CNTV 

GLOBAL_DEFINES += MMU_WITH_TRAMPOLINE=1 \

LINKER_SCRIPT += \
    $(BUILDDIR)/system-onesegment.ld

include make/module.mk
