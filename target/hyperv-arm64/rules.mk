LOCAL_DIR := $(GET_LOCAL_DIR)

GLOBAL_INCLUDES += \
    $(LOCAL_DIR)/include

PLATFORM := hyperv-arm64

#include make/module.mk
