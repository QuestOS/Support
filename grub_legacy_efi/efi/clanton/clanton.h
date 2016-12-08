/*
 * Copyright(c) 2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Contact Information:
 * Intel Corporation
 */

#ifndef GRUB_CLANTON_CLANTON_HEADER
#define GRUB_CLANTON_CLANTON_HEADER            1

/*
   Kernel command line token to be expanded into Quark UART1 MMIO address.
   Note the length of the token MUST be > "0xcafebabe", so as to prevent the
   rest of the string from being overwritten.
 */
#define QUARK_UART_MMIO_TOKEN                  "$EARLY_CON_ADDR_REPLACE"

/* Secure/non-secure boot switch.  */
extern unsigned short int grub_cln_secure;
/* Debug/release switch.  */
extern unsigned short int grub_cln_debug;

/* State whether the Grub was loaded from SPI/flash or SDIO.  */
extern unsigned short int grub_cln_loaded_from_spi;
/* State whether the Kernel must be fetched from SPI/flash or SDIO.  */
extern unsigned short int grub_cln_linux_spi;
/* State whether the Initrd must be fetched from SPI/flash or SDIO.  */
extern unsigned short int grub_cln_initrd_spi;

#endif /* ! GRUB_CLANTON_CLANTON_HEADER */
