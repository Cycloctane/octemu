======
octemu
======

Octane's chip-8 Emulator: A safe and efficient CHIP-8 emulator (interpreter) written in C.

Build
=====

Requirements: SDL3 (libsdl3-dev)::

    make octemu

Usage
=====

Start the emulator with a rom file::

    ./octemu ./rom.ch8

Keypad mapping
--------------

+-------+-------+-------+-------+
| 1 (1) | 2 (2) | 3 (3) | C (4) |
+-------+-------+-------+-------+
| 4 (Q) | 5 (W) | 6 (E) | D (R) |
+-------+-------+-------+-------+
| 7 (A) | 8 (S) | 9 (D) | E (F) |
+-------+-------+-------+-------+
| A (Z) | 0 (X) | B (C) | F (V) |
+-------+-------+-------+-------+

Space: Pause/Resume

Esc: Quit

F5: Reset the emulator and reload ROM

F12: Save screenshot to current directory

.. image:: img/tetris.png
    :alt: octemu running tetris
