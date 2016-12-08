#!/bin/bash
tar xvf gnu-efi_3.0r.orig.tar.gz
cd gnu-efi-3.0/gnuefi
make ARCH="ia32"
cd ../..
export GNUEFI_LIBDIR=`pwd`/gnu-efi-3.0/gnuefi/
cd ../grub_legacy_efi
autoreconf --install
export CC4GRUB='gcc -m32 -march=i586 -fno-stack-protector'
export CC="${CC4GRUB}"
./configure-clanton.sh
make

