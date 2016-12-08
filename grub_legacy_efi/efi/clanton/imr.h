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

#ifndef __IMR_H__
#define __IMR_H__

#include <grub/types.h>
#include <shared.h>

typedef enum imr_range
{
  IMR_RANGE_BOOT = 1,
  IMR_RANGE_KERN_TEXT = 3,
  IMR_RANGE_BZIMAGE = 7,
}imr_range;


/* setup IMR protection of the specified memory region */
grub_error_t intel_cln_imr_setup(imr_range id, grub_addr_t addr, grub_size_t size);

#endif /*__IMR_H__*/
