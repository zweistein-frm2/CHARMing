# CHARMing

a suite of applications for Neutron x-y data acquisition and processing using charm and mesytec hardware at the [FRM2](https://www.frm2.tum.de/startseite/)

linux and windows currently supported.

* charm: cmd line application for control of mesytec and charm hardware using the udp protocol. Real-time visualization of histograms either raw or binned. Main purpose is the initial setup and testing of the detector and settings. 

* entangle-charming: charm functionality within the [entangle device server framework](https://forge.frm2.tum.de/entangle/doc/entangle-master/). The charm functionality is implemented in a shared library to be used by python. Additionally full fault recovery is integrated and arbitrary polygon ROI (region of interests) can be defined for Neutron event counting.
* entangle-install-charming: helper program to install the entangle-charming devices support to [entangle](⇚).
* entangle-install-iseg-CC2x: helper program to install iseg HV power supply support to [entangle](⇚)
* charm-mesytec-emulator: helper program to emulate a real mesytec or charm device generating random data and supporting the same udp commands as the real hardware.
* nicos-install-charming:  helper program to install support for entangle-charming in [NICOS](https://www.nicos-controls.org/)

