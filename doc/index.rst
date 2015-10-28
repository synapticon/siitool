=======
Siitool
=======

Overview 
========

**Siitool** generates your SII/EEPROM files from ESI/XML description files, it also is capable of interpreting a raw SII/EEPROM file and print it in a human-friendly syntax.

.. cssclass:: downloadable-button 

  `Download Siitool <https://github.com/synapticon/siitool/archive/master.zip>`_

.. cssclass:: downloadable-button github

  `Visit Public Repository <https://github.com/synapticon/siitool>`_

Key Features
++++++++++++

* SII/EEPROM file generation from ESI/XML description file

* SII/EEPROM file interpretation and printing in a human-friendly syntax

How to use it
=============

Usage: siitool [-h] [-v] [-p] [-o outfile] [filename]

* -h print this help and exit
* -v print version an exit -o <name> write output to file <name>
* -p print content human readable

*filename path to eeprom file, if missing read from stdin*.

Recognized file types: SII and ESI/XML.

Examples
++++++++

sii_example.bin
```````````````

In this example we will read the `sii_example.bin <https://github.com/synapticon/siitool/raw/master/examples/sii_example.bin>`_ binary file and output it in a human readable syntax in our terminal:

.. code-block:: bash

	./siitool -p examples/sii_example.bin

.. note:: sii_example.bin is a binary file **not suitable for human reading**.

Somanet_CiA402-multi.xml
````````````````````````

In this example we will transform the `Somanet_CiA402-multi.xml <https://raw.githubusercontent.com/synapticon/siitool/master/examples/Somanet_CiA402-multi.xml>`_ definition file into a binary output, we will save it into a file, and a human friendly description will be shown on the terminal:

.. code-block:: bash

	./siitool -p -o cia402_single examples/Somanet_CiA402-single.xml

This definition file is a more complex CiA402 ESI which supports a set of PDOs for various CiA402 modes. Features:

* Mailbox
* Multiple PDOs

Somanet_CiA402-single.xml
`````````````````````````

In this example we will transform the `Somanet_CiA402-single.xml <https://raw.githubusercontent.com/synapticon/siitool/master/examples/Somanet_CiA402-single.xml>`_ definition file into a binary output and we will save it into a file:

.. code-block:: bash

	./siitool -o cia402_single examples/Somanet_CiA402-single.xml

This file is a simple CiA402 ESI. Features:

* Mailbox
* Single set of predefined PDOs

Somanet_CtrlProto.xml
`````````````````````

In this example we will transform the `Somanet_CtrlProto.xml <https://raw.githubusercontent.com/synapticon/siitool/master/examples/Somanet_CtrlProto.xml>`_ definition file into a binary output:

.. code-block:: bash

	./siitool examples/Somanet_CtrolProto.xml

This file is a simple ESI for use with controlproto. Features:

* No mailbox
* Single set of predefined PDOs

