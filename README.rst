=======
siitool
=======
(Maintainer: fjeschke[AT]synapticon[DOT]de)

This tool can View and print to screen theh contents of SII/EEPROM files and
ESI/XML files.  In addition it is possible to generate a valid SII binary from
a supported ESI file.

For more information please refer to the main documentation_.

.. _documentation : https://doc.synapticon.com/tools/siitool/doc/index.html

Install
=======

To build `siitool` please make sure to install `libxml2-dev` on
your system. The prefered way is with your local packet manager (eigher
(`apt(1)` or `rpm(1)` depending on your system).

Then simply do ::

  $ make

to build the `siitool`. After that it is recommendet to install the software
with ::

  $ sudo make install

This will install `siitool` to `/usr/local/bin` and will install a supportive
man page. To change the default install location simply change the `PREFIX`
variable in the Makefile to the location you prefer.

Licence
=======

Please see the LICENCE file in this repository.
