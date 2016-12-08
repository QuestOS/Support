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

#ifndef GRUB_CLANTON_ASSET_HEADER
#define GRUB_CLANTON_ASSET_HEADER            1

#include <clanton/sbh.h>

/* FIXME Quark software reference implementaion pads the CSBH to align to 1kB.
   This is hardcoded for now.  But needs to be read from CSBH lenght field.  */
#define SPI_CSBH_OFFS_HARDCODED              0x400

typedef enum
{
  GRUB_CLN_ASSET_KERNEL,
  GRUB_CLN_ASSET_INITRD,
  GRUB_CLN_ASSET_KERNEL_CSBH,
  GRUB_CLN_ASSET_INITRD_CSBH,
  GRUB_CLN_ASSET_CONFIG,
  GRUB_CLN_ASSET_CONFIG_CSBH,
} grub_cln_asset_type;

/* Access to an asset in read-only mode.  */
int grub_cln_asset_open (grub_cln_asset_type type, char *filename);
int grub_cln_asset_read (grub_cln_asset_type type, void *buf, int len);
void grub_cln_asset_seek (int offset);
int grub_cln_asset_size (grub_cln_asset_type type);
void grub_cln_asset_close (void);

/* Fetch and sanity check the Clanton Secure Boot Header.  */
int grub_cln_fetch_sbh (grub_cln_asset_type type, char *path,
                        struct grub_cln_sbh *csbh);

/* Dump the contents of layout.conf encoded in flash image */
void grub_cln_dump_layout (void);

#endif /* ! GRUB_CLANTON_ASSET_HEADER */
