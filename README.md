titan.TestPorts.E1TS
====================

This implements a TTCN-3 E1 Timeslot test port for Eclipse TITAN.  Running on a (Linux) computer,
it allows you to write TTCN-3 tests sending and receiving data on 64kBps E1 timeslots.

In order to interface both physical and virtual E1 lines, this port currently relies on
osmo-e1d, the Osmocom E1 daemon [1].  Adding back-ends for other drivers such as DAHDI should
not be hard, if needed at a later point.

The timeslots can either be opened in RAW (transparent) 64kBps mode, or with a HDLC controller.

The idea of this module is to be able to write abstract test suites in TTCN-3 which interface
IUT (Implementations under Test) over E1.


GIT repository
--------------

You can clone from the official titan.TestPorts.E1TS repository using

	git clone https://git.osmocom.org/titan.TestPorts.E1TS

There's a cgit interface at <https://cgit.osmocom.org/titan.TestPorts.E1TS/>

Documentation
-------------

This is still very much a Work-In-Progress, and hence there's no documentation yet, sorry.

References
----------

[1] osmo-e1d homepage at https://osmocom.org/projects/osmo-e1d/wiki
