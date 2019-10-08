.PHONY : all
.PHONY : clean
CURDIR=$(shell pwd)
CPUS=$$(($(shell cat /sys/devices/system/cpu/present | awk -F- '{ print $$2 }')+1))
KERNEL=$(CURDIR)/linux-source-4.19
BUILD=$(CURDIR)/build
OUTPUT=$(CURDIR)/output
COMPILER=aarch64-linux-gnu-

all: clean config kernel

config:
	$(Q)mkdir $(BUILD)
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 caninos7_defconfig
	
menuconfig:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 menuconfig
	
kernel:
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 dtbs
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 -j$(CPUS) Image modules
	$(Q)$(MAKE) -C $(KERNEL) O=$(BUILD) CROSS_COMPILE=$(COMPILER) ARCH=arm64 -j$(CPUS) INSTALL_MOD_PATH=$(BUILD) modules_install
	$(Q)rm -rf $(OUTPUT)
	$(Q)mkdir $(OUTPUT)
	$(Q)mkdir $(OUTPUT)/lib
	$(Q)cp -rf $(BUILD)/lib/modules $(OUTPUT)/lib/; find $(OUTPUT)/lib/ -type l -exec rm -f {} \;
	$(Q)cp $(BUILD)/arch/arm64/boot/Image $(OUTPUT)/
	$(Q)cp $(BUILD)/arch/arm64/boot/dts/caninos/v3sdc.dtb $(OUTPUT)/
	$(Q)cp $(BUILD)/arch/arm64/boot/dts/caninos/v3emmc.dtb $(OUTPUT)/

clean:
	$(Q)rm -rf build output
