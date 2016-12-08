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

#include <clanton/flash.h>
#include <clanton/intel_cln_sb.h>
#include <clanton/test_module.h>
#include <grub/cpu/linux.h>
#include <shared.h>

/* Test-specific parameters from kernel command line  */
#define TEST_PARAM_ID			"__cln_drive_failure="
#define TEST_PARAM_NO_KERNEL		"no-kernel"
#define TEST_PARAM_NO_RAMDISK		"no-ramdisk"
#define TEST_PARAM_NO_KERNEL_SIG	"no-kernel-signature"
#define TEST_PARAM_NO_RAMDISK_SIG	"no-ramdisk-signature"
#define TEST_PARAM_BAD_KERNEL_SIG	"kernel-sig-verification"
#define TEST_PARAM_BAD_RAMDISK_SIG	"ramdisk-sig-verification"
#define TEST_PARAM_BAD_IMR		"bad-imr"

/* Test setup and state  */
struct __cln_error_trigger_struct __cln_err = {
  .no_kernel = 0,
  .no_ramdisk = 0,
  .no_kernel_sig = 0,
  .no_ramdisk_sig = 0,
  .kernel_sig_fail = 0,
  .ramdisk_sig_fail = 0,
  .bad_imr = 0,
  .state_sign_verify = 0,
};

/* PUnit DMA registers over side-band.  */
#define PUNIT_SPI_DMA_COUNT_REG (0x60)
#define PUNIT_SPI_DMA_DEST_REG  (0x61)
#define PUNIT_SPI_DMA_SRC_REG   (0x62)

/* PUnit DMA block transfer size, in bytes.  */
#define SPI_DMA_BLOCK_SIZE 512

/* Buffer to DMA 1 block from SPI */
static grub_uint32_t spi_buffer[SPI_DMA_BLOCK_SIZE / sizeof (grub_uint32_t)];

/* Read from SPI via PUnit DMA engine.  */
static void
spi_dma_read (grub_uint32_t *src, grub_uint32_t *dst,
              grub_uint32_t dma_block_count)
{
  if (grub_cln_debug)
    {
      grub_printf ("%s: src=%p, dst=%p, count=%u\n", __func__, src, dst,
                  dma_block_count);
    }

  /* Setup source and destination addresses.  */
  intel_cln_sb_write_reg (SB_ID_PUNIT, CFG_WRITE_OPCODE, PUNIT_SPI_DMA_SRC_REG,
                          (grub_uint32_t) src);
  intel_cln_sb_write_reg (SB_ID_PUNIT, CFG_WRITE_OPCODE, PUNIT_SPI_DMA_DEST_REG,
                          (grub_uint32_t) dst);

  if (grub_cln_debug)
    {
      grub_printf ("%s: starting transaction\n", __func__);
    }

  /*
     Setup the number of block to be copied over.  Transaction will start as
     soon as the register is filled with value.
   */
  intel_cln_sb_write_reg (SB_ID_PUNIT, CFG_WRITE_OPCODE, PUNIT_SPI_DMA_COUNT_REG,
                          dma_block_count);

  /* Poll for completion.  */
  while (dma_block_count > 0)
    {
      intel_cln_sb_read_reg (SB_ID_PUNIT, CFG_READ_OPCODE, PUNIT_SPI_DMA_COUNT_REG,
                             &dma_block_count); 
    }

  if (grub_cln_debug)
    {
      grub_printf ("%s: transaction completed\n", __func__);
    }
}

/*
   Parse kernel command line for test-specific directives and set up test state
   variable accordingly.
 */
void
__cln_test_setup (char *arg)
{
  char *test_param = grub_strstr (arg, TEST_PARAM_ID);
  if (test_param)
    {
      test_param += grub_strlen(TEST_PARAM_ID);

      if (grub_strstr(test_param, TEST_PARAM_NO_KERNEL))
        __cln_err.no_kernel = 1;
      if (grub_strstr(test_param, TEST_PARAM_NO_RAMDISK))
        __cln_err.no_ramdisk = 1;
      if (grub_strstr(test_param, TEST_PARAM_NO_KERNEL_SIG))
        __cln_err.no_kernel_sig = 1;
      if (grub_strstr(test_param, TEST_PARAM_NO_RAMDISK_SIG))
        __cln_err.no_ramdisk_sig = 1;
      if (grub_strstr(test_param, TEST_PARAM_BAD_KERNEL_SIG))
        __cln_err.kernel_sig_fail = 1;
      if (grub_strstr(test_param, TEST_PARAM_BAD_RAMDISK_SIG))
        __cln_err.ramdisk_sig_fail = 1;
      if (grub_strstr(test_param, TEST_PARAM_BAD_IMR))
        __cln_err.bad_imr = 1;
    }
}

/*
   Force an IMR failure.
   This is achieved by DMAing a block of 512 bytes into an IMR where PUnit
   agent is prevented from accessing.
   First, sanity check that PUnit can DMA 512 bytes into a temporary buffer.
   Then, trigger an IMR violation by copying the same portion into bzImage
   IMR.
 */
void
__cln_test_imr (void)
{
  grub_uint32_t *p = (grub_uint32_t *) GRUB_LINUX_BZIMAGE_ADDR;

  if (! __cln_err.bad_imr)
    return;

  spi_dma_read ((grub_uint32_t *) GRUB_CLN_MFH_ADDR, spi_buffer, 1);
  grub_printf ("%s: PUnit DMAing %uB into temp buffer passed\n", __func__,
               SPI_DMA_BLOCK_SIZE);

  grub_printf ("%s: PUnit DMAing %uB into non-PUnit IMR @%p\n", __func__,
               SPI_DMA_BLOCK_SIZE, p);
  spi_dma_read ((grub_uint32_t *) GRUB_CLN_MFH_ADDR, p, 1);
  grub_printf ("%s: BUG: PUnit DMA to non-PUnit IMR didn't fail!\n", __func__);
}

