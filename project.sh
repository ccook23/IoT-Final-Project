#!/bin/sh

qemu-system-arm \
  -M versatilepb \
  -kernel zImage \
  -drive file=rootfs.ext2,if=scsi,format=raw \
  -append "rootwait root=/dev/sda console=ttyAMA0,115200" \
  -serial stdio \
  -net nic \
  -net user,hostfwd=tcp::2222-:22
