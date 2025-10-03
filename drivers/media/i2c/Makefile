# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2023 Intel Corporation.

ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

KERNEL_VERSION := $(shell echo $(KERNELRELEASE) | sed 's/[^0-9.]*\([0-9.]*\).*/\1/')

version_lt = $(shell \
    v1=$(1); \
    v2=$(2); \
    IFS='.'; \
    set -- $$v1; i=$$1; j=$$2; k=$$3; \
    set -- $$v2; a=$$1; b=$$2; c=$$3; \
    if [ "$$i" -lt "$$a" ]; then \
        echo "true"; \
    elif [ "$$i" -eq "$$a" ] && [ "$$j" -lt "$$b" ]; then \
        echo "true"; \
    elif [ "$$i" -eq "$$a" ] && [ "$$j" -eq "$$b" ] && [ "$$k" -lt "$$c" ]; then \
        echo "true"; \
    else \
        echo "false"; \
    fi)

KV_I2C_MAX9X_COMPAT := 6.3.0

ccflags-y += -I$(srcpath)/$(src)/../../../include/
ccflags-y += -DCONFIG_VIDEO_D4XX_SERDES -DPLATFORM_AXIOMTEK
ifeq ($(CONFIG_VIDEO_INTEL_IPU6), m)
	ccflags-y += -DCONFIG_VIDEO_INTEL_IPU6=1
endif

obj-$(CONFIG_VIDEO_IMX390) += imx390.o
obj-$(CONFIG_VIDEO_AR0234) += ar0234.o
obj-$(CONFIG_VIDEO_LT6911UXC) += lt6911uxc.o
obj-$(CONFIG_VIDEO_LT6911UXE) += lt6911uxe.o
obj-$(CONFIG_VIDEO_ISX031) += isx031.o
ifeq ($(call version_lt,$(KERNEL_VERSION),$(KV_I2C_MAX9X_COMPAT)),false)
obj-$(CONFIG_VIDEO_MAX9X) += max9x/
endif
obj-$(CONFIG_VIDEO_D4XX_MAX9295) += d4xx-max9295.o
obj-$(CONFIG_VIDEO_D4XX_MAX9296) += d4xx-max9296.o
obj-$(CONFIG_VIDEO_D4XX_MAX96724) += d4xx-max96724.o
obj-$(CONFIG_VIDEO_D4XX) += d4xx.o
