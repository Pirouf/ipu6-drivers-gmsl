ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

ccflags-y += -I$(srcpath)/$(src)/../../pci/intel/
ccflags-y += -DCONFIG_BACKWARD_INTEL_ISYS

obj-m		+= serdes.o
obj-m		+= max9295.o
obj-m		+= max9296.o
