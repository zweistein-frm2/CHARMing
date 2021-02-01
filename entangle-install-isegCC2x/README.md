# entangle-install-isegCC2x


this software installs the interface to the [entangle framework](
https://forge.frm2.tum.de/entangle/doc/entangle-master/build/)
for high-voltage modules of the CCR series from the company [Iseg spezialelektronik gmbh](https://iseg-hv.com/en/home)

## Linux installation procedure:

1. [Install entangle framework](https://forge.frm2.tum.de/wiki/projects:tango:install_entangle) :
by default this will install a globally available "entangle-server" command. Needed .res files must be put in /etc/entangle (sudo needed)

2. run "sudo entangle-install-segCC2x" (sudo needed) , file available from intranet on  [taco24](ftp://172.25.2.24/x86_64/) or build it from sources.
3. you have to copy the needed .res file from /etc/entangle/iseg/CC2xlib/example/ to /etc/entangle. For example use Erwin-both.res
4. Edit Erwin-both.res to check parameters for iseg connection (ip address, port, password)
5. run "entangle-server Erwin-both.res"


## intelligent features: transitions, groups and operatingstyles

it is possible to define complex transition scenarios,  for example ramp up channel 1 to 100V, wait until reached, then ramp up group Anodes (for example channels 4,5,7) to 50V. See Alanspecs_11Sept2020.pdf . Some examples are given in the .res files in the CC2xlib/example subdirectory.
The IntelligentPowersupply features can be controlled via its StringIO interface 