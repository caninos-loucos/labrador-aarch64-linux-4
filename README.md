# labrador-linux-64

Linux 4 kernel source code for Caninos Labrador.

## About

This repository contains the source code for both the Caninos Labrador's
 64-bit and 32-bit Linux kernels, along with Debian-specific patches and
 security updates.

## Clean Build

Before compilation, please ensure that you have the following libraries and/or
 tools installed on your machine:
 
1) GNU Binutils environment and Native GCC compiler
2) GCC cross-compiler targeting "gcc-aarch64-linux-gnu"
3) GCC cross-compiler targeting "gcc-arm-linux-gnueabihf"
4) Make build tool
5) Git client
6) Bison and Flex development libraries
7) NCurses development libraries
8) LibSSL development libraries
9) U-Boot Tools libraries

```
$ sudo apt-get update
$ sudo apt-get install build-essential binutils make git
$ sudo apt-get install gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu
$ sudo apt-get install bison flex libncurses-dev libssl-dev u-boot-tools
```

After installing these tools, clone this repository to your computer.
 Then, navigate to its main directory and execute its makefile.

```
$ git clone https://github.com/caninos-loucos/labrador-linux-6.git
$ cd labrador-linux-6
$ make all
$ make all32
```

## Incremental Build (64-bit)

If you want to perform an incremental build, follow these steps:

1) To load the 64-bit configuration:

```
make config
```

> Note: This will overwrite any previously configured settings.

2) To customize which modules are compiled into your kernel image:

```
make menuconfig
```

3) To compile the device tree binary blob:

```
make dtbs
```

4) To compile your kernel image:

```
make kernel
```

5) To reset all configurations:

```
make clean
```

## Kernel

After successful compilation, the kernel should be located in the "output"
 folder for 64-bit architectures or in the "output32" folder for
 32-bit architectures.

## Contributing

**Caninos Loucos Forum: <https://forum.caninosloucos.org.br/>**

**Caninos Loucos Website: <https://caninosloucos.org/>**
