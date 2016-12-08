#!/bin/sh

die()
{
    printf 'Fatal: '
    printf "$@"
    exit 1
}

# Points to the directory where "crt0-efi-ia32.o" is (e.g., /usr/lib32)
# Can be overriden on the command line: GNUEFI_LIBDIR=/usr/lib32 ./configure ...
: ${GNUEFI_LIBDIR:=/p/clanton/users/software/clanton-target-libs/installroot/usr/local/lib}

[ -d "${GNUEFI_LIBDIR}" ] || die 'GNUEFI_LIBDIR=%s   not found\n' "${GNUEFI_LIBDIR}"


# Yocto's environment-setup-i586-poky-linux-uclibc defines a CC looking like this:
# export CC='gcc -m32 -march=i586'
$CC --version || die 'CC undefined or not pointing to a compiler\n'


# Append to generic CFLAGS from  environment-setup-i586-poky-linux-uclibc
# Just like the GRUB.bb recipe does
export CFLAGS="$CFLAGS -Os -fno-strict-aliasing -Wall -Werror -Wno-shadow -Wno-unused -Wno-pointer-sign -DGRUB_CLN_DEBUG=1 -DINTEL_CLN_TEST=1"

./configure --host=i586-poky-linux-uclibc --without-curses --disable-auto-linux-mem-opt --with-platform=efi --libdir="${GNUEFI_LIBDIR}"
