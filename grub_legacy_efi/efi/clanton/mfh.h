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

#ifndef GRUB_CLANTON_MFH_HEADER
#define GRUB_CLANTON_MFH_HEADER     1

#include <grub/types.h>
#include <shared.h>

/* Magic number corresponds to "_MFH" in ASCII.  */
#define GRUB_CLN_MFH_IDENTIFIER                 0x5F4D4648

/* Clanton Master Flash Header.  */
typedef struct grub_cln_mfh
{
  grub_uint32_t identifier;                       /* 0x000 */
  grub_uint32_t version;                          /* 0x004 */
  grub_uint32_t flags;                            /* 0x008 */
  grub_uint32_t next_header_block;                /* 0x00C */
  grub_uint32_t flash_item_count;                 /* 0x010 */
  grub_uint32_t boot_prio_list_count;             /* 0x014 */
  /* Pad to 512 bytes.  */
  grub_uint8_t  padding[0x1E8];                   /* 0x018 */
}
*grub_cln_mfh_t;


/* Flash item types.  */
#define CLN_MFH_ITEM_TYPE_FW_STAGE1                     0x00000000
#define CLN_MFH_ITEM_TYPE_FW_STAGE1_SIGNED              0x00000001
/* Reserved.  */
#define CLN_MFH_ITEM_TYPE_FW_STAGE2                     0x00000003
#define CLN_MFH_ITEM_TYPE_FW_STAGE2_SIGNED              0x00000004
#define CLN_MFH_ITEM_TYPE_FW_STAGE2_CONFIG              0x00000005
#define CLN_MFH_ITEM_TYPE_FW_STAGE2_CONFIG_SIGNED       0x00000006
#define CLN_MFH_ITEM_TYPE_FW_PARAMS                     0x00000007
#define CLN_MFH_ITEM_TYPE_FW_RECOVERY                   0x00000008
#define CLN_MFH_ITEM_TYPE_FW_RECOVERY_SIGNED            0x00000009
/* Reserved.  */
#define CLN_MFH_ITEM_TYPE_BOOTLOADER                    0x0000000B
#define CLN_MFH_ITEM_TYPE_BOOTLOADER_SIGNED             0x0000000C
#define CLN_MFH_ITEM_TYPE_BOOTLOADER_CONFIG             0x0000000D
#define CLN_MFH_ITEM_TYPE_BOOTLOADER_CONFIG_SIGNED      0x0000000E
/* Reserved.  */
#define CLN_MFH_ITEM_TYPE_KERNEL                        0x00000010
#define CLN_MFH_ITEM_TYPE_KERNEL_SIGNED                 0x00000011
#define CLN_MFH_ITEM_TYPE_RAMDISK                       0x00000012
#define CLN_MFH_ITEM_TYPE_RAMDISK_SIGNED                0x00000013
/* Reserved.  */
#define CLN_MFH_ITEM_TYPE_LOADABLE_PROGRAM              0x00000015
#define CLN_MFH_ITEM_TYPE_LOADABLE_PROGRAM_SIGNED       0x00000016
/* Reserved.  */
#define CLN_MFH_ITEM_TYPE_BUILD_INFO                    0x00000018

/* Flash item definition.  */
typedef struct grub_cln_mfh_item
{
  grub_uint32_t type;                             /* 0x000 */
  grub_uint32_t flash_item_addr;                  /* 0x004 */
  grub_uint32_t flash_item_len;                   /* 0x008 */
  grub_uint32_t reserved;                         /* 0x00C */
}
*grub_cln_mfh_item_t;

/* Load the MFH into memory.  */
void grub_cln_mfh_load (const struct grub_cln_mfh *mfh);

/* Lookup the MFH for a specific entry_type.
   The MMIO start address of the MFH is passed by *mfh_addr.
   If the entry is found, store the MMIO address into *addr and the length
   into *len and return ERR_NONE.
   Return error code if any error.
   Note it automatically loads the MFH if not already done so.  */
grub_error_t grub_cln_mfh_entry_lookup (const grub_uint8_t *mfh_addr,
                                        unsigned int entry_type,
                                        grub_uint8_t **addr,
                                        grub_uint32_t *len);

#endif /* ! GRUB_CLANTON_MFH_HEADER */
