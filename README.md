# Fablock

This is the software for a door-locking controller featuring a pin-pad for
authorization entry, a beep speaker for feedback, a motor locking/unlocking
 the door, two sensors detecting door closing and successful locking and
 a USART uplink to higher-level authentication logic living on a raspberry pi.

## Configurations

There are several instaces for this procect.
Configure the instance in the makefile:

* Upper
    This is the door to the bikeworkshop.
    Enable it by specifying CONFIG=UPPER
* Demo Board
    This is the configuration for the evaluation setup.
    Enable it by specifying CONFIG=DEMO
* Cellar
    This is the door to the Cellar door.
    Enable it by specifying CONFIG=CELLAR
* Social Room
    This is the placeholder for configuration for the Social Room.
    Enable it by specifying CONFIG=SOCIAL



## lockserver

Included is also the higher-level daemon in lockserver/ and the openscad files
 for the pin-pad casing.

## statustool

Small statustool that decodes the output from the serial port of the arduino