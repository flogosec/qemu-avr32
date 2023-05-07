===========
QEMU AVR32 README
===========
I added support for the AVR32 instruction set to QEMU.
In May 2023 I will add hardware support for the Nanomin A3200 board. For now, I added an example board that can be used to test the implementation.



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

