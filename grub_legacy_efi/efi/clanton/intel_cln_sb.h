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

#ifndef __INTEL_CLN_SB_HEADER__
#define __INTEL_CLN_SB_HEADER__

#include <clanton/clanton.h>
#include <clanton/sbh.h>
#include <clanton/target.h>
#include <grub/types.h>

typedef enum {
  SB_ID_HUNIT     = 0x03,
      SB_ID_PUNIT     = 0x04,
      SB_ID_ESRAM     = 0x05,
      SB_ID_SEC_FUSE  = 0x33, /* Fuse banks  */
}cln_sb_id;

/* Sideband MCR opcodes */
#define CFG_READ_FUSE_OPCODE     (0x06) /* Fuse read  */
#define CFG_READ_OPCODE          (0x10) /* Register read  */
#define CFG_WRITE_OPCODE         (0x11) /* Register write  */

int intel_cln_sb_probe (void);
void intel_cln_sb_read_reg(cln_sb_id id, grub_uint8_t cmd, grub_uint8_t reg,
                           grub_uint32_t *data);
void intel_cln_sb_write_reg(cln_sb_id id, grub_uint8_t cmd, grub_uint8_t reg,
                            grub_uint32_t data);
#endif /* __INTEL_CLN_SB_HEADER__ */

