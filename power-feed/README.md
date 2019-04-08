# `power-feed` 

**Single-axis Arduino-based milling machine power feed controller.**

_Ported from Chris Mower's original millcode project (https://github.com/kintekobo/millcode). Pin-compatible with the original._ 

## Features

* Rapid and Precision feedrates.
* Iconographic display of current mode.
* Control via physical buttons _or_ over the serial port/USB:
    * `stop` - All-stop
    * `fl` - Fast-left movement
    * `sl` - Slow/Precision-left movement
    * `sr` - Slow/Precision-right movement
    * `fr` - Fast-right movement
    * `1000` - Set the precision feedrate to 1000 micrometers per second.
* Hardware timer-based pulse generation with endstop and safe-stop detection before pulse generation.
* Endstop backoff (see `ALLOW_BACK_AWAY_FROM_ENDSTOP` in `defs.h`) - optionally allow moving in the opposite direction after triggering the endstop.

## Hardware

Written to work the original M542T stepper motor driver and I2C (PCF8574 I2C IO expander + Hitachi LCD controller) display backpack.

For the rotary encoder, two 100nF capacitors between the quadrature outputs and ground are _essential_.

## Libraries

You'll need the [LiquidCrystal_I2C](https://github.com/lucasmaziero/LiquidCrystal_I2C) library to use the same display used in the original project, however it would be easy on one of the higher pin-count Arduinos to switch to the traditional Hitachi parallel LCD controller. There aren't enough pins for this on the Uno/mega328p though.

No other libraries are used.
