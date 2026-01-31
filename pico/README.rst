===========
octemu pico
===========

Octane's CHIP-8/SUPER-CHIP Emulator for Raspberry Pi Pico. Built with octemu core and
RPi Pico SDK.

Usage
=====

ROMs are embedded into the firmware and selectable in the menu. (Use 1,2,3 on keypad to
switch and choose ROM).

Build
=====

Requirements:

* CMake
* Raspberry Pi Pico SDK and toolchain (arm-none-eabi compiler and libc)
* python3, jinja2, pyyaml (for parsing config and embedding ROM files)

To add custom ROM files, config them in ``games.yml`` file following the existing format
and pass it to ``OCTEMU_PICO_ROM`` option::

    git submodule update --init
    cmake -B build -DOCTEMU_PICO_ROM=./games.yml -DPICO_BOARD=pico
    cmake --build build

If this option is not set, octemu pico will use all compatible ROMs from `chip8Archive
<https://github.com/JohnEarnest/chip8Archive>`__.

Wiring
======

Display
-------

128x64 SH1106 SPI OLED display:

=======  ==============
Display       GPIO
=======  ==============
SCK      GP2 (SPI0 SCK)
SDA      GP3 (SPI0 TX)
RES      GP4
DC       GP5
CS       GND
=======  ==============

Input
-----

* 8pin 4x4 matrix keypad:

  * Row: GP6-GP9
  * Column: GP10-GP13

* Optional buttons (Active HIGH):

  * Pause/Resume: GP14
  * Reset: GP15
  * Back to menu: GP16

Sound
-----

* Passive buzzer: GP20

Serial
------

* GP0-GP1 (UART0) for printing additional messages

Only enabled in debug build (``-DCMAKE_BUILD_TYPE=Debug``).
