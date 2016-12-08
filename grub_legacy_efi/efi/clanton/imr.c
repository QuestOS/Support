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

#include <stdbool.h>
#include <grub/types.h>
#include <shared.h>
#include <grub/cpu/linux.h>
#include <clanton/asset.h>
#include <clanton/intel_cln_sb.h>
#include <clanton/test_module.h>
#include "imr.h"

#define DRAM_IMR1L		(0x44)  /* IMR1L address */
#define DRAM_IMR1H		(0x45)  /* IMR1H address */
#define DRAM_IMR1RM		(0x46)  /* IMR1RM address */
#define DRAM_IMR1WM		(0x47)  /* IMR1WM address */
#define DRAM_IMR3L		(0x4C)  /* IMR3L address */ 
#define DRAM_IMR3H		(0x4D)  /* IMR3H address */ 
#define DRAM_IMR3RM		(0x4E)  /* IMR3RM address */
#define DRAM_IMR3WM		(0x4F)  /* IMR3WM address */
#define DRAM_IMR7L		(0x5C)  /* IMR7L address */ 
#define DRAM_IMR7H		(0x5D)  /* IMR7H address */ 
#define DRAM_IMR7RM		(0x5E)  /* IMR7RM address */
#define DRAM_IMR7WM		(0x5F)  /* IMR7WM address */

#define LOCK			true	
#define UNLOCK			false
#define IMR_MIN_SIZE		(0x400)
#define IMR_LOCK_BIT		(0x80000000)
/* Mask of the last 2 bit of IMR address [23:2] */
#define IMR_REG_MASK		(0xFFFFFC)
/* default register value */ 
#define IMR_WRITE_ENABLE_ALL	(0xFFFFFFFF)
/* default register value */
#define IMR_READ_ENABLE_ALL	(0xBFFFFFFF)
/* Mask that enables IMR access for Non-SMM Core, Core Snoops Only.*/
#define IMR_SNOOP_NON_SMM_ENABLE	(0x40000001)
/* Mask that enables IMR access for Non-SMM Core Only.*/
#define IMR_NON_SMM_ENABLE		(0x00000001)

/* IMRs are 1kB-aligned */
#define IMR_ALIGNMENT			10
/* Right shift of 22-bit IMR addr to fit IMR Lo/Hi register addr field */
#define IMR_ADDR_SHIFT			8

/**
 * imr_align
 *
 * @param addr: memory addr
 *
 * make input memory addr to be 1k aligned
 * The IMR designed as always protect the extra 1k memory space based on input
 * high reg value so the input memory address round down here
 */
static inline grub_uint32_t
imr_align(grub_uint32_t addr)
{
  addr &= (~((1 << IMR_ALIGNMENT) - 1));
  return addr;
}

/**
 * intel_cln_imr_write 
 *
 * @param lo_addr: starting memory addr
 * @param hi_addr: end memory addr
 * @param imr_l: IMRXL reg addr
 * @param imr_h: IMRXH reg addr
 * @param imr_rm: IMR read mask reg addr
 * @param imr_wm: IMR write mask reg addr
 *
 * write in imr memory value to corresponding register addr.
 */
static void 
intel_cln_imr_write(grub_uint32_t lo_addr, grub_uint32_t hi_addr,
                    grub_uint8_t imr_l, grub_uint8_t imr_h,
                    grub_uint8_t imr_rm, grub_uint8_t imr_wm, bool lock)
{
  grub_uint32_t tmp_addr;

  /* We have to becareful here, some IMR regions may previously used by
   * BIOS.
   * 1. disable the IMR if its already enabled
   * 2. assign IMR Low address to where you want
   * 3. assign IMR High address to where you want
   * 4. apply read/write access  masks
   */
  intel_cln_sb_read_reg(SB_ID_ESRAM, CFG_READ_OPCODE, imr_l, &tmp_addr);
  if(tmp_addr & IMR_LOCK_BIT)
    {
      grub_printf("%s IMR has already locked.\n ",__func__);
      return;
    }

  if(tmp_addr)
    {
      if (grub_cln_debug)
        grub_printf("%s IMR already in use, start at: 0x%08x \n",
                    __func__, tmp_addr);
      intel_cln_sb_write_reg(SB_ID_ESRAM, CFG_WRITE_OPCODE, imr_rm,
                             IMR_READ_ENABLE_ALL );
      intel_cln_sb_write_reg(SB_ID_ESRAM, CFG_WRITE_OPCODE, imr_wm,
                             IMR_WRITE_ENABLE_ALL);
    }

  intel_cln_sb_write_reg(SB_ID_ESRAM, CFG_WRITE_OPCODE, imr_h, hi_addr);
  if (grub_cln_debug)
    grub_printf("%s IMRXH  0x%08x\n", __func__, hi_addr);

  intel_cln_sb_write_reg(SB_ID_ESRAM, CFG_WRITE_OPCODE, imr_l, lo_addr);
  if (grub_cln_debug)
    grub_printf("%s IMRXL  0x%08x\n", __func__, lo_addr);

  /* authrised agents to access initrd: Non-SMM(0b), Host(30b), PUnit(29b) */
  /* NOTE. CPU snoop will be always writes */
  intel_cln_sb_write_reg(SB_ID_ESRAM, CFG_WRITE_OPCODE, imr_rm,
                         IMR_NON_SMM_ENABLE);
  intel_cln_sb_write_reg(SB_ID_ESRAM, CFG_WRITE_OPCODE, imr_wm,
                         IMR_SNOOP_NON_SMM_ENABLE);

  if(lock)
    {
      lo_addr |= IMR_LOCK_BIT;
      intel_cln_sb_write_reg(SB_ID_ESRAM, CFG_WRITE_OPCODE, imr_l, lo_addr);
      if (grub_cln_debug)
        grub_printf("%s IMRXL locked  0x%08x\n", __func__, lo_addr);
    }

}

/**
 * intel_cln_imr_setup
 *
 * @param id: IMR ID
 * @param addr: starting addr 
 * @param size: length of memory   
 *
 * setup IMR protection of the specified memory region.
 */
grub_error_t 
intel_cln_imr_setup(imr_range id, grub_addr_t addr, grub_size_t size)
{

  /* The steps to setup IMR in Grub
   * 1. calculate the memory address
   * 2. memory alignment to 1k
   * 3. shift address value to register specified format.
   * 4. read register to see if its already locked.
   * 5. enable default read/write all access right if imr already in use
   * 6. write high/low memory address to IMR registers.
   * 7. setup authorised agents for IMR mask.
   */
  grub_uint32_t imr_hi_addr;
  grub_uint32_t imr_lo_addr;

  errnum = ERR_NONE;

  if(size < IMR_MIN_SIZE)
    {
      grub_printf("Invalid input size! \n ");
      errnum = ERR_BAD_ARGUMENT;
      return errnum;
    }
  else
    {
      if (grub_cln_debug)
        grub_printf("setting imr with input:  addr=0x%08x, size=0x%x \n",
                    addr, (unsigned int)size);
    }

  /* align to 1k boundary */
  imr_lo_addr = imr_align(addr);

  /* update high memory address */
  imr_hi_addr = imr_lo_addr + imr_align(size);

  /* align to 1k boundary */
  imr_hi_addr = imr_align(imr_hi_addr);

  /* apply IMR MASK for register specified format */
  imr_hi_addr = ((imr_hi_addr >> IMR_ADDR_SHIFT) & IMR_REG_MASK);
  imr_lo_addr = ((imr_lo_addr >> IMR_ADDR_SHIFT) & IMR_REG_MASK);

  switch(id)
  {
  /* IMR for boot params */
  case IMR_RANGE_BOOT:
    {
      intel_cln_imr_write(imr_lo_addr, imr_hi_addr, DRAM_IMR1L, DRAM_IMR1H,
                          DRAM_IMR1RM, DRAM_IMR1WM, UNLOCK);
      break;
    }

    /* IMR for bzImage */
  case IMR_RANGE_BZIMAGE:
    {
      intel_cln_imr_write(imr_lo_addr, imr_hi_addr, DRAM_IMR7L, DRAM_IMR7H,
                          DRAM_IMR7RM, DRAM_IMR7WM, UNLOCK);
      break;
    }
  default:
    {
      grub_printf("%s Invalid input ! \n", __func__);
      errnum = ERR_BAD_ARGUMENT;
      break;
    }
  }

  return errnum;
}
/* EOL */
