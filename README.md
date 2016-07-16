# linux-am2320-driver
Linux Device Driver for AM2302

**Cleaning the directory**
make clean

**Building**
make

**Inserting into kernel**
sudo insmod am2320.ko
then do,
echo am2320 0x5c > /sys/class/i2c-adapter/i2c-1/new_device

**To remove from kernel**
echo 0x5c > /sys/class/i2c-adapter/i2c-1/delete_device
then do,
sudo rmmod am2320

**Viewing measurements**
cat /sys/bus/i2c/devices/1-005c/temp1_input
cat /sys/bus/i2c/devices/1-005c/humidity1_input
OR
/sys/class/i2c-adapter/i2c-1/1-005c/temp1_input
/sys/class/i2c-adapter/i2c-1/1-005c/humidity1_input