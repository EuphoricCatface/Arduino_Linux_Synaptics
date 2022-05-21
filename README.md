Arduino Linux Synaptics
=========

PS/2 Synaptics as a Bluetooth HID device, with Arduino Nano as the controller, with the code adapted from Linux Device Driver as PS/2 driver, with HC-05 module as the bluetooth controller

Linux driver was retrieved from: https://github.com/torvalds/linux/tree/e4ce4d3a939d97bea045eafa13ad1195695f91ce/drivers/input/mouse  
RN-42 mouse/keyboard driver was retrieved from: https://github.com/evankale/ArduinoBTPS2  
Inspired by: https://www.instructables.com/Bluetooth-Keyboard-Mouse-Adapter/  
Inspired by (2): https://mitxela.com/projects/bluetooth_hid_gamepad

Synaptics Library (CC-0) was retrieved from: https://github.com/spad/Synaptics  
A few functions added, published at: https://github.com/EuphoricCatface/Synaptics

My device (looks like [this](https://m.media-amazon.com/images/I/71acj+dVZtL._AC_SL1500_.jpg)) was salvaged from 2012 Clevo OEM laptop. It was hooked up to a "PS/2 to USB adpater" before, but it wouldn't produce any scroll signal that way. As it turns out, at least on this device, you need to activate Synaptics' own protocol and parse the information.

The code currently only supports my old device, but `Arduino_Linux_Synaptics.cpp` contains entirety of packet manipulation of Linux Kernel. It should be easier to start from here, should you attempt to revive one's full potential.

`Arduino_Linux_Synaptics.cpp` interprets the packets and produces position data of fingers. Verbosity of the information may depend on the version of your device.

`Arduino_Linux_Synaptics.ino` turns the position data into information needed for bluetooth mouse packet, and hands it over to `Bluetooth.cpp`. A number of gestures are implemented, namely tap, double tap, tap to drag for single, double and triple taps, and two finger drag to scroll.

