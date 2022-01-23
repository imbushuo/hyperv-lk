#!/bin/bash
qemu-system-aarch64 -cpu cortex-a57 -m 512 -smp 1 -machine virt,gic-version=3 -kernel build-qemu-virt-arm64-test/lk_s.elf -net none -nographic

