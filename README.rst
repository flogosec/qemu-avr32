===========
QEMU AVR32 README
===========
I added support for the AVR32 instruction set to QEMU.
This branch contains an implementation for the Nanomind A3200 board.


Building
========
The build process is not changed. To only compile the AVR32 target do:

.. code-block:: shell

  ../configure --target-list=avr32-softmmu
  make -j 16

Additional information can also be found online via the QEMU website:

* `<https://wiki.qemu.org/Hosts/Linux>`_
* `<https://wiki.qemu.org/Hosts/Mac>`_
* `<https://wiki.qemu.org/Hosts/W32>`_



Using the emulation
========
After you successfully build QEMU AVR32, you can use it like this:

.. code-block:: shell

  build/avr32-softmmu/qemu-system-avr32 -machine nanomind-a3200 -bios [path to avr32 binary file]