[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/inet-framework/inet)

[![Build: Linux](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/build-linux.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/build-linux.yml)
[![Build: Mac OS](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/build-macos.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/build-macos.yml)
[![Build: Windows](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/build-windows.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/build-windows.yml)

[![Test: chart](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/chart-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/chart-tests.yml)
[![Test: feature](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/feature-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/feature-tests.yml)
[![Test: fingerprint](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/fingerprint-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/fingerprint-tests.yml)
[![Test: module](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/module-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/module-tests.yml)
[![Test: other](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/other-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/other-tests.yml)
<!-- [![Test: speed](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/speed-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/speed-tests.yml) -->
[![Test: statistical](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/statistical-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/statistical-tests.yml)
[![Test: unit](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/unit-tests.yml)
[![Test: validation](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/validation-tests.yml/badge.svg)](https://github.com/aarizaq/inetmanet-4.x/actions/workflows/validation-tests.yml)

Inetmanet/INET Framework for OMNEST/OMNeT++
=================================

The [INET framework](https://inet.omnetpp.org) is an open-source communication networks
simulation package, written for the [OMNEST/OMNeT++](https://omnetpp.org) simulation system.
The INET framework contains models for numerous wired and wireless protocols, a detailed
physical layer model, application models and more. See the CREDITS file for the
names of people who have contributed to the INET Framework.

IMPORTANT: The INET Framework is continuously being improved: new parts
are added, bugs are corrected, and so on. We cannot assert that any protocol
implemented here will work fully according to the specifications. YOU ARE
RESPONSIBLE YOURSELF FOR MAKING SURE THAT THE MODELS YOU USE IN YOUR SIMULATIONS
WORK CORRECTLY, AND YOU'RE GETTING VALID RESULTS.

Contributions are highly welcome. You can make a difference!

See the WHATSNEW file for recent changes.


GETTING STARTED
---------------
You may start by downloading and installing the INET framework. Read the INSTALL
file for further information.

Then you can gather initial experience by starting some examples or following a
tutorial or showcase (see the /examples, /showcases or /tutorials folder).
After that, you can learn the NED language from the OMNeT++ manual & sample
simulations.

After that, you may write your own topologies using the NED language. You may
assign some of the submodule parameters in NED files. You may leave some of
them unassigned.

Then, you may assign unassigned module parameters in omnetpp.ini of your
simulation. (You can refer to sample simulations & manual for the content of
omnetpp.ini)

Finally, you will be ready to run your simulation. As you see, you may use
the INET framework without writing any C++ code, as long as you use the
available modules.

To implement new protocols or modify existing ones, you'll need to add your
code somewhere under the src directory. If you add new files under the 'src/inet'
directory you will need to regenerate the makefiles (using the 'make makefiles'
command).

If you want to use external interfaces in INET, enable the "Emulation" feature
either in the IDE or using the inet_featuretool then regenerate the INET makefile
using 'make makefiles'.

INETMANET differences
=================================
Inetmanet is compatible with inet framework but it has several differences.

       Additional routing protocols:
              Batman.
              Olsr (Universidad de Murcia).
              Dymo (Friedrich-Alexander-Universität version).
              Aodv (Uppsala university).
              Dsr (Uppsala university).
              (Except Dsr, the rest of protocols are implemented like Udp applications).

       Basic support for SixLowPan
       
       Lora model:
               Flora.
          
       Cloud Computing modules:
              iCanClod.
              CloudNetSim++.

