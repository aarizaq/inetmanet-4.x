Multicast Streams with Failure Protection
=========================================

Goals
-----

In this example, we replicate the multicast stream example from the IEEE 802.1 CB standard.

| Verified with INET version: ``4.4``
| Source files location: `inet/showcases/tsn/framereplication/multicastfailureprotection <https://github.com/inet-framework/inet/tree/master/showcases/tsn/framereplication/multicastfailureprotection>`__

The Model
---------

In this configuration, we use a network of TSN switches. A multicast stream is
sent through the network from one of the switches to all other switches.

Here is the network:

.. figure:: media/Network.png
   :align: center

Here is the configuration:

.. literalinclude:: ../omnetpp.ini
   :language: ini

Results
-------

.. The following video shows the behavior in Qtenv:

   .. video:: media/behavior.mp4
      :align: center
      :width: 90%

   Here are the simulation results:

   .. .. figure:: media/results.png
      :align: center
      :width: 100%

Sources: :download:`omnetpp.ini <../omnetpp.ini>`, :download:`MulticastFailureProtectionShowcase.ned <../MulticastFailureProtectionShowcase.ned>`


Try It Yourself
---------------

If you already have INET and OMNeT++ installed, start the IDE by typing
``omnetpp``, import the INET project into the IDE, then navigate to the
``inet/showcases/tsn/framereplication/multicastfailureprotection`` folder in the `Project Explorer`. There, you can view
and edit the showcase files, run simulations, and analyze results.

Otherwise, there is an easy way to install INET and OMNeT++ using `opp_env
<https://omnetpp.org/opp_env>`__, and run the simulation interactively.
Ensure that ``opp_env`` is installed on your system, then execute:

.. code-block:: bash

    $ opp_env run inet-4.5 --init -w inet-workspace --install --build-modes=release --chdir \
       -c 'cd inet-4.5.*/showcases/tsn/framereplication/multicastfailureprotection && inet'

This command creates an ``inet-workspace`` directory, installs the appropriate
versions of INET and OMNeT++ within it, and launches the ``inet`` command in the
showcase directory for interactive simulation.

Alternatively, for a more hands-on experience, you can first set up the
workspace and then open an interactive shell:

.. code-block:: bash

    $ opp_env install --init -w inet-workspace --build-modes=release inet-4.5
    $ cd inet-workspace
    $ opp_env shell

Inside the shell, start the IDE by typing ``omnetpp``, import the INET project,
then start exploring.

Discussion
----------

Use `this <https://github.com/inet-framework/inet/discussions/790>`__ page in the GitHub issue tracker for commenting on this showcase.

