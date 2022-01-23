# Hyper-V ARM64 Guest Target

This is a proof-of-concept target for Hyper-V ARM64, with bare minimum peripherals:

* PL011 Serial Input/Output
* GICv3 (Unicore)
* Arch Timer
* PSCI Reboot/Poweroff

There's still a bunch of things to do, but so far this is sufficient for me to do
some experiments with Hyper-V ARM64 Guests.

# Boot Requirements

* Windows 11 Insider Builds after 2021/08 (because it uses full Arch Timer which is added after 2021/08)
* At least 3580MB memory for guest
* At least one CPU
* An ELF loader with VA->PA fixup

# TODO

* Remove `SMC` assumption for PSCI calls (might have targets that need to use `HVC`)
* Investigate PL011 input lag
