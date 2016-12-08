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

/*
 * This file implements two early consoles named earlycln0 earlycln1
 * this just lets use get text out the PCI UART early in boot, since Quark SoC
 * does not implement traditional 0x3f8 type UARTs
 */

#include <clanton/clanton.h>
#include <grub/types.h>
#include <netboot/linux-asm-io.h>
#include <netboot/timer.h>
#include <shared.h>
#include <stage2/serial.h>
#include "early_uart.h"

#define PCI_VENDOR_ID		(0x00)
#define PCI_DEVICE_ID		(0x02)
#define PCI_CLASS_DEVICE	(0x0A)

#define PCI_VENDOR_ID_INTEL	(0x8086)
#define PCI_DEVICE_CLNUART	(0x0936)
#define UART_BARMAP_LEN		(0x10)

static grub_uint32_t *pclnuart = NULL;

static grub_uint32_t read_pci_config(grub_uint8_t bus, grub_uint8_t slot,
				     grub_uint8_t func, grub_uint8_t offset)
{
	grub_uint32_t v;
	outl(0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset, 0xcf8);
	v = inl(0xcfc);
	return v;
}

static grub_uint16_t read_pci_config_16(grub_uint8_t bus, grub_uint8_t slot,
					grub_uint8_t func, grub_uint8_t offset)
{
	grub_uint16_t v;
	outl(0x80000000 | (bus<<16) | (slot<<11) | (func<<8) | offset, 0xcf8);
	v = inw(0xcfc + (offset&2));
	return v;
}

/**
 * clnuart_early_setup
 *
 * Sets up one of the Clanton UARTs as an early boot console
 */
static void cln_uart_early_setup(int num, int slot, int func, int offset)
{
	grub_uint32_t addr = 0;
	addr = read_pci_config(num, slot, func, offset);

	if(addr & 0x00000001){
		/* This is an IO bar */
		grub_printf("bailing.. this is an IO bar\n");
		return;
	}
	if(addr & 0x00000006){
		/* Diver expects 32 bit range */
		grub_printf("bailing.. driver expects 32 bit range\n");
		return;
	}
	pclnuart = (grub_uint32_t *) (addr&0xFFFFFFF0);
}

/**
 * clnuart_early_probe
 *
 * Do an early probe of the PCI bus - find Clanton UARTs
 */
static int cln_early_uart_probe(int num, int slot, int func)
{
	grub_uint16_t class;
	grub_uint16_t vendor;
	grub_uint16_t device;

	class = read_pci_config_16(num, slot, func, PCI_CLASS_DEVICE);

	if (class == 0xffff) {
		return -1;
	}

	vendor = read_pci_config_16(num, slot, func, PCI_VENDOR_ID);
	device = read_pci_config_16(num, slot, func, PCI_DEVICE_ID);

	/* Do early PCI UART init */
	if(vendor == PCI_VENDOR_ID_INTEL){
		/* UART0 is F1, UART1 is F5. We're probing UART1.  */
		if (device == PCI_DEVICE_CLNUART && 0x0005 == func){
			cln_uart_early_setup(num, slot, func, 0x10);
			return 0;
		}
	}
	return -1;
}

grub_uint32_t *cln_early_uart_init(void)
{
	int bus = 0, slot = 0, func = 0;

	for (slot = 0; slot < 32; slot++){
		for (func = 0; func < 8; func++) {
		/* Only probe function 0 on single fn devices */
			if(cln_early_uart_probe(bus, slot, func) == 0){
				if (grub_cln_debug) {
					grub_printf("%s: UART @ B/D/F "
						    "%d/%d/%d\n",
						    __func__, bus, slot, func);
				}
				return pclnuart;
			}
		}
	}
	errnum = ERR_DEV_VALUES;
	return NULL;
}

