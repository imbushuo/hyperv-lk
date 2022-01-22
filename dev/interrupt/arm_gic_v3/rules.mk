LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/arm_gic_v3.c \
	$(LOCAL_DIR)/GICv3Impl.c \
	$(LOCAL_DIR)/GICv3Impl.S

include make/module.mk
