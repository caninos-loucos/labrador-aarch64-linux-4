
CURDIR=$(shell pwd)
CPUS=$$(($(shell cat /sys/devices/system/cpu/present | awk -F- '{ print $$2 }')+1))
KERNEL=$(CURDIR)/linux-source-4.19
BUILD=$(CURDIR)/build
BUILD32=$(CURDIR)/build32
OUTPUT=$(CURDIR)/output
OUTPUT32=$(CURDIR)/output32
COMPILER=aarch64-linux-gnu-
COMPILER32=arm-linux-gnueabihf-

.PHONY: all config menuconfig dtbs kernel clean 

all: clean config kernel
all32: clean32 config32 kernel32

config32:
	$(Q)mkdir $(BUILD32)
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm caninos5_defconfig

menuconfig32:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm menuconfig
	
dtbs32:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm dtbs
	$(Q)mkdir -p $(OUTPUT32)
	$(Q)rm -rf $(OUTPUT32)/*.dtb
	$(Q)cp $(BUILD32)/arch/arm/boot/dts/caninos-k5.dtb $(OUTPUT32)/kernel.dtb
	
kernel32: dtbs32
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm -j$(CPUS) uImage modules
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD32) CROSS_COMPILE=$(COMPILER32) ARCH=arm -j$(CPUS) INSTALL_MOD_PATH=$(BUILD32) modules_install
	$(Q)rm -rf $(OUTPUT32)/lib
	$(Q)mkdir -p $(OUTPUT32)/lib
	$(Q)cp -rf $(BUILD32)/lib/modules $(OUTPUT32)/lib/; find $(OUTPUT32)/lib/ -type l -exec rm -f {} \;
	$(Q)rm -f $(OUTPUT32)/uImage
	$(Q)cp $(BUILD32)/arch/arm/boot/uImage $(OUTPUT32)/
	
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
	$(Q)rm -rf $(OUTPUT)/*.dtb
	$(Q)cp $(BUILD)/arch/arm64/boot/dts/caninos/*.dtb $(OUTPUT)/
	
kernel: dtbs
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 -j$(CPUS) Image modules
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 -j$(CPUS) INSTALL_MOD_PATH=$(BUILD) modules_install
	$(Q)rm -rf $(OUTPUT)/lib
	$(Q)mkdir -p $(OUTPUT)/lib
	$(Q)cp -rf $(BUILD)/lib/modules $(OUTPUT)/lib/; find $(OUTPUT)/lib/ -type l -exec rm -f {} \;
	$(Q)rm -f $(OUTPUT)/Image
	$(Q)cp $(BUILD)/arch/arm64/boot/Image $(OUTPUT)/

clean:
	$(Q)rm -rf $(BUILD)
	
