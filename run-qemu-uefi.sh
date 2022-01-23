#!/bin/bash
qemu-system-aarch64 -cpu cortex-a57 -m 512 -smp 1 -machine virt,gic-version=3 -kernel ../edk2/Build/ArmVirtQemuKernel-AARCH64/DEBUG_GCC5/FV/QEMU_EFI.fd -net none -nographic -drive if=none,file=/mnt/f/Scratch/ELFLoaderTest.vhd,id=hd0 -device virtio-blk-device,drive=hd0


