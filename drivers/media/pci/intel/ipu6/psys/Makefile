# SPDX-License-Identifier: GPL-2.0-only

is_kernel_lt_6_10 = $(shell if [ $$(printf "6.10\n$(KERNELVERSION)" | sort -V | head -n1) != "6.10" ]; then echo 1; fi)
ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

intel-ipu6-psys-objs			+= ipu-psys.o \
					   ipu-fw-psys.o \
					   ipu-fw-resources.o \
					   ipu-resources.o \
					   ipu6-psys.o \
					   ipu6-ppg.o \
					   ipu6-l-scheduler.o \
					   ipu6-fw-resources.o \
					   ipu6se-fw-resources.o \
					   ipu6ep-fw-resources.o

obj-$(CONFIG_VIDEO_INTEL_IPU6)		+= intel-ipu6-psys.o

ccflags-y += -DCONFIG_BACKWARD_INTEL_PSYS
ccflags-y += -I$(srcpath)/$(src)/../ipu6/
ccflags-y += -I$(srcpath)/$(src)/../
ccflags-y += -I$(srcpath)/$(src)/../../
