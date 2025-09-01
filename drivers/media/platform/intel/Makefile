# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 - 2022 Intel Corporation.

is_kernel_lt_6_14 = $(shell if [ $$(printf "6.14\n$(KERNELVERSION)" | sort -V | head -n1) != "6.14" ]; then echo 1; fi)
ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ifeq ($(is_kernel_lt_6_14), 1)
ccflags-y += -I$(srcpath)/$(src)/../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../../pci/intel/
else
ccflags-y += -I$(M)/drivers/media/pci/intel/ -I$(M)/include/
endif

obj-$(CONFIG_INTEL_IPU6_ACPI)         += ipu6-acpi.o \
					ipu6-acpi-pdata.o \
					ipu6-acpi-common.o

ccflags-y += -DCONFIG_BACKWARD_INTEL_ISYS
CFLAGS_ipu6-acpi-pdata.o += -DDEBUG
CFLAGS_ipu6-acpi-common.o += -DDEBUG
CFLAGS_ipu6-acpi.o += -DDEBUG
