ifneq ($(KERNELRELEASE),)
obj-m := retrociaa_eeprom.o
else
KDIR := $(HOME)/linux-kernel-labs/src/linux
all:
	$(MAKE) -C $(KDIR) M=$$PWD
endif
