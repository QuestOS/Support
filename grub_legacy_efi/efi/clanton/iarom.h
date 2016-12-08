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

#ifndef GRUB_CLANTON_IAROM_HEADER
#define GRUB_CLANTON_IAROM_HEADER            1

#include <clanton/clanton.h>
#include <clanton/sbh.h>
#include <clanton/target.h>
#include <grub/types.h>

/* Perform signature verification.
   TODO needs proper documentation.  */
int grub_cln_verify_asset_signature (grub_uint8_t *addr);

#endif /* ! GRUB_CLANTON_IAROM_HEADER */
