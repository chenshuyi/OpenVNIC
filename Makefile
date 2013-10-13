#
# Makefile for VNIC layer
#

obj-m := vnic.o

vnic-objs := vnic_core.o vnic_proc.o vnic_dev.o

PWD := $(shell pwd)
KERNELDIR ?= # Your own kernel path

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

