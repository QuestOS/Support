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

#include <clanton/asset.h>
#include <clanton/clanton.h>
#include <clanton/flash.h>
#include <clanton/mfh.h>
#include <clanton/sbh.h>
#include <clanton/test_module.h>
#include <grub/types.h>
#include <shared.h>

#define grub_file_size()    filemax

/* Flash item MMIO address & length.  */
static grub_uint8_t *cln_flash_item_addr = 0x0;
static grub_uint32_t cln_flash_item_len = 0x0;
static unsigned int spi_offs_intra_module = 0x0;
static grub_uint8_t skip_csbh = 0;

/* The path to the CSBH file.  */
static char cln_sbh_path[GRUB_CLN_SBH_FILE_PATHMAX] = "";

/* Return the path to the CSBH file matching the module.  */
static char *
cln_get_sbh_fs_path (char *string)
{
  char *pos = NULL;
  int len = 0;
  char *path = grub_strtok_r (string, " ", &pos);

  cln_sbh_path[0] = 0;

  len = grub_strlen (path) + grub_strlen (GRUB_CLN_SBH_FILE_EXT);
  if (len < GRUB_CLN_SBH_FILE_PATHMAX)
    {
      grub_strncat (cln_sbh_path, path, GRUB_CLN_SBH_FILE_PATHMAX);
      grub_strncat (cln_sbh_path, GRUB_CLN_SBH_FILE_EXT,
                    GRUB_CLN_SBH_FILE_PATHMAX);
    }
  else
    grub_printf ("%s error: path to CSBH file is too long\n", __func__);

  return cln_sbh_path;
}

/* Sanity check offset and size of MFH item.
   Minimum size is passed as an argument.  Maximum size is determined by
   boundary check of the flash part.  */
static int
cln_flash_item_is_sane (grub_uint32_t min_size)
{
  /* Take into account the Clanton SBH if in secure mode.  */
  min_size += (grub_cln_secure ? sizeof (struct grub_cln_sbh) : 0);

  /* Check that:
     1. minimum size is satisfied
     2. extent of the asset doesn't wrap around the address space.  */
  if (cln_flash_item_len < min_size
      || (grub_uint32_t) cln_flash_item_addr + cln_flash_item_len
         < (grub_uint32_t) cln_flash_item_addr)
    {
      grub_printf ("flash item size is outside the accepted range\n");
      errnum = ERR_FILELENGTH;
      return 0;
    }

  return 1;
}

#define KERNEL 0
#define INITRD 1
#define CONFIG 2

static int
spi_open (int type)
{
  skip_csbh = 0;
  spi_offs_intra_module = 0;

  /* For non-secure boot, look up unsigned assets first.  If not found, look up
     signed assets then.  */
  switch (type)
  {
  case KERNEL:
    grub_cln_mfh_entry_lookup (grub_cln_mfh_addr,
                               grub_cln_secure ? CLN_MFH_ITEM_TYPE_KERNEL_SIGNED
                               : CLN_MFH_ITEM_TYPE_KERNEL,
                               &cln_flash_item_addr,
                               &cln_flash_item_len);
    if (0 == grub_cln_secure && ERR_FILE_NOT_FOUND == errnum)
      {
        errnum = ERR_NONE;
        grub_cln_mfh_entry_lookup (grub_cln_mfh_addr,
                                   CLN_MFH_ITEM_TYPE_KERNEL_SIGNED,
                                   &cln_flash_item_addr,
                                   &cln_flash_item_len);
        if (ERR_NONE == errnum)
          skip_csbh = 1;
      }
    if (ERR_NONE == errnum)
      cln_flash_item_is_sane (sizeof (struct linux_kernel_header));
    break;
  case INITRD:
    grub_cln_mfh_entry_lookup (grub_cln_mfh_addr,
                               grub_cln_secure ? CLN_MFH_ITEM_TYPE_RAMDISK_SIGNED
                               : CLN_MFH_ITEM_TYPE_RAMDISK,
                               &cln_flash_item_addr,
                               &cln_flash_item_len);
    if (0 == grub_cln_secure && ERR_FILE_NOT_FOUND == errnum)
      {
        errnum = ERR_NONE;
        grub_cln_mfh_entry_lookup (grub_cln_mfh_addr,
                                   CLN_MFH_ITEM_TYPE_RAMDISK_SIGNED,
                                   &cln_flash_item_addr,
                                   &cln_flash_item_len);
        if (ERR_NONE == errnum)
          skip_csbh = 1;
      }
    if (ERR_NONE == errnum)
      cln_flash_item_is_sane (0);
    break;
  default:
    /* case CONFIG  */
    grub_cln_mfh_entry_lookup (grub_cln_mfh_addr,
                               grub_cln_secure ?
                               CLN_MFH_ITEM_TYPE_BOOTLOADER_CONFIG_SIGNED
                               : CLN_MFH_ITEM_TYPE_BOOTLOADER_CONFIG,
                               &cln_flash_item_addr,
                               &cln_flash_item_len);
    if (0 == grub_cln_secure && ERR_FILE_NOT_FOUND == errnum)
      {
        errnum = ERR_NONE;
        grub_cln_mfh_entry_lookup (grub_cln_mfh_addr,
                                   CLN_MFH_ITEM_TYPE_BOOTLOADER_CONFIG_SIGNED,
                                   &cln_flash_item_addr,
                                   &cln_flash_item_len);
        if (ERR_NONE == errnum)
          skip_csbh = 1;
      }
    if (ERR_NONE == errnum)
      cln_flash_item_is_sane (0);
    break;
  }

  /* If the asset is signed, seek past CSBH.  */
  if (grub_cln_secure || skip_csbh)
    spi_offs_intra_module += SPI_CSBH_OFFS_HARDCODED;

  return (ERR_NONE == errnum);
}

int
grub_cln_asset_open (grub_cln_asset_type type, char *filename)
{
  /* No error by default.  */
  int ret = 1;

  __cln_test_asset(type);

  switch (type)
  {
  case GRUB_CLN_ASSET_KERNEL:
    if (grub_cln_linux_spi)
      ret = spi_open (KERNEL);
    else
      ret = grub_open (filename);
    break;
  case GRUB_CLN_ASSET_INITRD:
    if (grub_cln_initrd_spi)
      ret = spi_open (INITRD);
    else
      ret = grub_open (filename);
    break;
  case GRUB_CLN_ASSET_CONFIG:
    if (grub_cln_loaded_from_spi)
      ret = spi_open (CONFIG);
    else
      ret = grub_open (filename);
    break;
  case GRUB_CLN_ASSET_KERNEL_CSBH:
    if (grub_cln_linux_spi)
      ret = spi_open (KERNEL);
    else
      ret = grub_open (cln_get_sbh_fs_path (filename));
    break;
  case GRUB_CLN_ASSET_INITRD_CSBH:
    if (grub_cln_initrd_spi)
      ret = spi_open (INITRD);
    else
      ret = grub_open (cln_get_sbh_fs_path (filename));
    break;
  default:
    /* case GRUB_CLN_ASSET_CONFIG_CSBH: */
    if (grub_cln_loaded_from_spi)
      ret = spi_open (CONFIG);
    else
      ret = grub_open (cln_get_sbh_fs_path (filename));
    break;
  }

  return ret;
}

int
grub_cln_asset_read (grub_cln_asset_type type, void *buf, int len)
{
  int read = 0;

  switch (type)
  {
  case GRUB_CLN_ASSET_KERNEL:
    if (grub_cln_linux_spi)
      {
        grub_memcpy (buf,
                     cln_flash_item_addr
                     + spi_offs_intra_module, len);
        read = len;
        spi_offs_intra_module += read;
      }
    else
      read = grub_read (buf, len);
    break;
  case GRUB_CLN_ASSET_KERNEL_CSBH:
    if (grub_cln_linux_spi)
      {
        grub_memcpy (buf, cln_flash_item_addr, len);
        read = len;
      }
    else
      read = grub_read (buf, len);
    break;
  case GRUB_CLN_ASSET_INITRD:
    if (grub_cln_initrd_spi)
      {
        grub_memcpy (buf,
                     cln_flash_item_addr + spi_offs_intra_module,
                     len);
        read = len;
        spi_offs_intra_module += read;
      }
    else
      read = grub_read (buf, len);
    break;
  case GRUB_CLN_ASSET_INITRD_CSBH:
    if (grub_cln_initrd_spi)
      {
        grub_memcpy (buf, cln_flash_item_addr, len);
        read = len;
      }
    else
      read = grub_read (buf, len);
    break;
  case GRUB_CLN_ASSET_CONFIG:
    if (grub_cln_loaded_from_spi)
      {
        grub_memcpy (buf,
                     cln_flash_item_addr + spi_offs_intra_module,
                     len);
        read = len;
        spi_offs_intra_module += read;
      }
    else
      read = grub_read (buf, len);
    break;
  default:
    /* case GRUB_CLN_ASSET_CONFIG_CSBH  */
    if (grub_cln_loaded_from_spi)
      {
        grub_memcpy (buf, cln_flash_item_addr, len);
        read = len;
      }
    else
      read = grub_read (buf, len);
    break;
  }

  return read;
}

void
grub_cln_asset_seek (int offset)
{
  /* Note it doesn't differentiate between module types, as it's only used
     by the linux loader routine.  */
  if (grub_cln_linux_spi)
    {
      if (grub_cln_secure || skip_csbh)
        spi_offs_intra_module = offset + SPI_CSBH_OFFS_HARDCODED;
      else
        spi_offs_intra_module = offset;
    }
  else
    grub_seek (offset);
}

int
grub_cln_asset_size (grub_cln_asset_type type)
{
  int size = -1;

  switch (type)
  {
  case GRUB_CLN_ASSET_KERNEL:
    if (grub_cln_linux_spi)
      {
        size = cln_flash_item_len;
        if (grub_cln_secure || skip_csbh)
          size -= SPI_CSBH_OFFS_HARDCODED;
      }
    else
      size = grub_file_size ();
    break;
  case GRUB_CLN_ASSET_INITRD:
    if (grub_cln_initrd_spi)
      {
        size = cln_flash_item_len;
        if (grub_cln_secure || skip_csbh)
          size -= SPI_CSBH_OFFS_HARDCODED;
      }
    else
      size = grub_file_size ();
    break;
  default:
    /* case GRUB_CLN_ASSET_CONFIG  */
    if (grub_cln_loaded_from_spi)
      {
        size = cln_flash_item_len;
        if (grub_cln_secure || skip_csbh)
          size -= SPI_CSBH_OFFS_HARDCODED;
      }
    else
      size = grub_file_size ();
    break;
  }

  return size;
}

void grub_cln_asset_close (void)
{
  grub_close ();
  return;
}

int
grub_cln_fetch_sbh (grub_cln_asset_type type, char *path,
                    struct grub_cln_sbh *csbh)
{
  int success = 1;

  /* Fetch the Clanton SBH.  */
  if (! grub_cln_asset_open (type, path))
    success = 0;
  else if (sizeof (*csbh) != grub_cln_asset_read (type, csbh, sizeof (*csbh)))
    {
      errnum = ERR_READ;
      grub_close ();
      grub_printf ("cannot read the Clanton SBH");
      success = 0;
    }
  grub_close ();

  /* Check for valid SBH.  */
  if (success)
    {
      if (GRUB_CLN_SBH_MAGIC_NUMBER != csbh->security_header.magic_number)
        {
          errnum = ERR_EXEC_FORMAT;
          grub_printf ("invalid CSBH magic number\n");
          success = 0;
        }
    }

  return success;
}

/* Dump the contents of layout.conf encoded in flash image */
void grub_cln_dump_layout (void)
{
  grub_uint8_t * data = 0x0;
  grub_uint32_t len = 0x0;

  errnum = ERR_NONE;
  grub_cln_mfh_entry_lookup (grub_cln_mfh_addr,
                             CLN_MFH_ITEM_TYPE_BUILD_INFO,
                             &data,
                             &len);
  if (errnum != ERR_NONE)
    {
      errnum = ERR_NONE;
      grub_printf("cannot find layout.conf MFH entry!\n");
      return;
    }

  grub_printf("Found layout.conf @ 0x%08x len 0x%08x\n", data, len);
  grub_printf("%.*s\n", len, data);
}
