
CURDIR=$(shell pwd)
CPUS=$$(($(shell cat /sys/devices/system/cpu/present | awk -F- '{ print $$2 }')+1))
KERNEL=$(CURDIR)/linux-source-4.19
BUILD=$(CURDIR)/build
BUILD32=$(CURDIR)/build32
OUTPUT=$(CURDIR)/output
COMPILER=aarch64-linux-gnu-
COMPILER32=arm-linux-gnueabihf-

.PHONY: all config menuconfig dtbs kernel clean 

all: clean config kernel



config32:
	$(Q)mkdir $(BUILD32)
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm caninos5_defconfig

menuconfig32:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm menuconfig
	
kernel32:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm -j$(CPUS) uImage modules
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm -j$(CPUS) INSTALL_MOD_PATH=$(BUILD32) modules_install
	
clean32:
	$(Q)rm -rf $(BUILD32)
	

config:
	$(Q)mkdir $(BUILD)
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 caninos7_defconfig
	
menuconfig:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 menuconfig
	
dtbs:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 dtbs
	$(Q)mkdir -p $(OUTPUT)
	$(Q)cp $(BUILD)/arch/arm64/boot/dts/caninos/v3sdc.dtb $(OUTPUT)/
	$(Q)cp $(BUILD)/arch/arm64/boot/dts/caninos/v3emmc.dtb $(OUTPUT)/
	$(Q)cp $(BUILD)/arch/arm64/boot/dts/caninos/v3psci.dtb $(OUTPUT)/
	
kernel: dtbs
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 -j$(CPUS) Image modules
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 -j$(CPUS) INSTALL_MOD_PATH=$(BUILD) modules_install
	$(Q)rm -rf $(OUTPUT)/lib
	$(Q)mkdir -p $(OUTPUT)/lib
	$(Q)cp -rf $(BUILD)/lib/modules $(OUTPUT)/lib/; find $(OUTPUT)/lib/ -type l -exec rm -f {} \;
	$(Q)cp $(BUILD)/arch/arm64/boot/Image $(OUTPUT)/

clean:
	$(Q)rm -rf $(BUILD)
	
