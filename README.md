# linux-am2320-driver
Linux Device Driver for AM2320 

**Experimental**

Tested on a Raspberry pi 3 with kernel version 4.4.14-v7+

Usage
------
You need to have the proper kernel headers installed to build this driver. Use this tool to install kernel headers https://github.com/notro/rpi-source

1. clone this repo 
2. git clone linux-am2320-driver
3. cd into directory
4. type make
5. sudo insmod am2320.ko
6. type lsmod and see whether the driver is loaded properly
7. sudo bash
8. if you have a new Raspberry pi (B+, 2 or 3)

  echo am2320 0x5c > /sys/class/i2c-adapter/i2c-1/new_device

  else if this is a Rev. 1

  echo am2320 0x5c > /sys/class/i2c-adapter/i2c-0/new_device

Viewing measurements
======================
cat /sys/bus/i2c/devices/1-005c/temp1_input

cat /sys/bus/i2c/devices/1-005c/humidity1_input

OR

cat /sys/class/i2c-adapter/i2c-1/1-005c/temp1_input

cat /sys/class/i2c-adapter/i2c-1/1-005c/humidity1_input

To remove from kernel
=====================
sudo bash
echo 0x5c > /sys/class/i2c-adapter/i2c-1/delete_device

then do,

rmmod am2320

Cleaning the directory
=======================
make clean

