# DS75-temperature-sensor-driver

Driver for temperature sensor chip DS75
Written to be used with Raspberry Pi 3 via GPIO

This project has been done purely for educational purposes. There is a previously existing driver for this device.
I have not taken a look of that code however, instead I have studied Linux driver tutorials at the general level.


# Wirings

RPI3-GPIO       DS75 sensor chip

SDA & SCL-------SDA & SCL
GND-------------GND
VCC-3.3v--------VCC
GNDx3-----------ADDRESS PINS x3

Using GNDs gives an address 000
Note that additional pull-up resistors are not needed with Raspberry Pi 3


# Device tree overlay installation

1. Install device-tree-compiler
2. Add the following line to /boot/config.txt
dtoverlay=DS75
3. Convert dts file to dtbo file by using this command:
dtc -@ -I dts -O dtb -o DS75_overlay.dtbo DS75_overlay.dts
4. Move .dtbo file to the directory /boot/overlays

# Commands for the driver

NOTE: the register pointer changes every time you change configurations or otherwise write to registers

p0 - set register pointer to read the temperature register
p1 - ...to the configuration register
p2 - ...to the t_hyst register
p3 - ...to the t_os register

r0 - set temperature (and t_hyst and t_os) register resolution to 9-bit
r1 - ...to 10-bit
r2 - ...to 11-bit
r3 - ...to 12-bit

f1, f2, f4, f6 - set fault tolerance to the corresponding number

o0 - switch thermostat output polarity to active low
o1 - switch thermostat output polarity to active high

m0 - switch thermostat operating mode to comparator mode
m1 - switch thermostat operating mode to interrupt mode

s0 - active conversion and thermostat operation
s1 - shutdown mode on

tX - write value X to the t_hyst register

xX - write value X to the t_os register

aX - switch device address (selected via device's address pins), values 0-7 possible, 0 is default

# Command examples for testing

sudo insmod DS75_driver.ko
sudo cat /dev/DS7500
sudo chmod 666 /dev/DS7500
sudo echo p2 >> /dev/DS7500
sudo echo o1 >> /dev/DS7500
etc.
