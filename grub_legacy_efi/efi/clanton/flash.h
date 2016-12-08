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

#ifndef GRUB_CLANTON_FLASH_HEADER
#define GRUB_CLANTON_FLASH_HEADER            1

#include <clanton/target.h>
#include <grub/types.h>

/* Pointer to MFH.  */
extern grub_uint8_t *grub_cln_mfh_addr;

/* MMIO address of MFH.  */
#define GRUB_CLN_MFH_ADDR          0xFFF08000

/* MMIO address of signed Key Module.  */
#define GRUB_CLN_S_KEYMOD_ADDR     0xFFFD8000
 
#endif /* ! GRUB_CLANTON_FLASH_HEADER */
