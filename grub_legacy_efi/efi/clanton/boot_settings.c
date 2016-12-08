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
#include <clanton/iarom.h>
#include <clanton/intel_cln_sb.h>
#include <clanton/target.h>
#include <clanton/sbh.h>
#include <grub/efi/efi.h>
#include <grub/efi/misc.h>
#include <grub/misc.h>
#include <shared.h>

/* Constants related to 'spi_rom_fuse_in' fuse.  */
#define SPI_ROM_FUSE_REG_OFFS   0x25
#define SPI_ROM_FUSE_REG_MASK   0x00000010

/* Pointer to memory-mapped MFH address.  */
grub_uint8_t *grub_cln_mfh_addr = (grub_uint8_t *) GRUB_CLN_MFH_ADDR;

/* Indicate if the GRUB has been loaded from SPI or SDIO.  */
unsigned short int grub_cln_loaded_from_spi = 0;

/* The (signed) configuration file buffer.  Its maximum size is limited to
   8kB.  */
static char cfg_buffer[0x2000] = "";

/* Function used to load grub.conf from SD or SPI*/
static int do_load_config_file(char **cfg_file_buffer, int *cfg_file_size, char *source);

/*
   Determine whether or not Secure Boot is enabled.
 */
void
grub_cln_detect_secure_sku (void)
{
  grub_uint32_t spi_rom_fuse_range = 0x0;

  /* Read the spi_rom_fuse_in 32-bit fuse range from Fuse Bank 0.  */
  intel_cln_sb_read_reg (SB_ID_SEC_FUSE, CFG_READ_FUSE_OPCODE,
                         SPI_ROM_FUSE_REG_OFFS, &spi_rom_fuse_range); 

/* FIXME On Clanton, spi_rom_fuse_in == 1 if Secure Boot is enabled.  On
   emulation platform instead, the bit is swapped.  The following #if's are
   an emulation workaround.  Restore when we transition to real silicon.  */
#if 0
  if (spi_rom_fuse_range & SPI_ROM_FUSE_REG_MASK)
#else
  if (! (spi_rom_fuse_range & SPI_ROM_FUSE_REG_MASK))
#endif
    grub_cln_secure = 1;
  else
    grub_cln_secure = 0;

  if (grub_cln_debug)
    grub_printf("Detected %ssecure SKU\n", grub_cln_secure ? "" : "non-");
}

void
grub_cln_load_config_file (char **cfg_file_buffer, int *cfg_file_size)
{
  grub_efi_loaded_image_t *loaded_image = NULL;
  unsigned long drive = 0, partition = 0;
  char *next = 0;
  int mount_failed = 0;
  int load_conf_failed = 0;
  char *ext_fs = "File system Device";
  char *spi_fs = "SPI"; 
  // We first start by looking for grub.conf in SDIO. If we do not find it there, then look in SPI/Flash

  /* Set path to the config file.  */
  /* Since we boot grub from SPI, config_file will only be
  "grub.conf" instead of the expected "/boot/grub/grub.conf"
  This is due to how config_file is assigned, at boot time it's
  hardcoded to "/boot/grub/menu.lst" and when grub executes it
  gets the path to its own image and redefines config_file to
  be [path_to_image]/grub.conf.  In our case, as grub is found
  on SPI the path is null, resulting in config_file = grub.conf
  This means that the grub.conf will need to be in the top most
  directory of the SD As of now though, we're using a hardcoded
  "/boot/grub/grub.conf" as this adheres to the grub standard.
  */
  grub_set_config_file ("/boot/grub/grub.conf");
 
  grub_memset (cfg_buffer, 0x0, sizeof (cfg_buffer));

  // Try to mount first mass storage partition returned by the BIOS

  next = set_device ("(hd0,0)");
  if (!next) 
    {
      mount_failed = 1;
    }

  if (!open_device () && errnum != ERR_FSYS_MOUNT)
    {
      mount_failed = 1;
    }

  errnum = 0;
  saved_partition = current_partition;
  saved_drive = current_drive;

  // Try to load config file from (hd0,0)

  load_conf_failed = do_load_config_file(cfg_file_buffer, cfg_file_size, ext_fs);


  // In case we can't load it from (hd0,0) load from SPI/Flash if possible.

  if (mount_failed || load_conf_failed)
    {
      errnum = 0;
       /* Find out whether the Grub was loaded from SPI/flash or SDIO.  */
       /* FIXME this logic needs to be moved earlier, to detect media as soon as
          possible.  Ideally to be moved in stage2.c  */
       loaded_image = grub_efi_get_loaded_image (grub_efi_image_handle);
       grub_cln_loaded_from_spi =
           ! grub_get_drive_partition_from_bdev_handle (loaded_image->device_handle,
                                                        &drive, &partition);

       do_load_config_file(cfg_file_buffer, cfg_file_size, spi_fs);
    }


  if (grub_cln_debug)
    grub_printf ("GRUB loaded from %s\n",
                 grub_cln_loaded_from_spi ? "SPI/Flash" : "file system device");

}

static int
do_load_config_file(char **cfg_file_buffer, int *cfg_file_size, char *source) // from SD or SPI
{
  int read = 0;
  struct grub_cln_sbh *sbh = NULL;
  grub_uint32_t sbh_len = 0;

  *cfg_file_buffer = cfg_buffer;

  if (grub_cln_secure)
    {
      sbh = (struct grub_cln_sbh *)cfg_buffer;
      if (! grub_cln_fetch_sbh (GRUB_CLN_ASSET_CONFIG_CSBH, config_file, sbh))
        {
          errnum = ERR_SGN_FILE_NOT_FOUND;
          return 0;
	}
      sbh_len = sbh->security_header.header_len;
    }

  /* Open the configuration file.  */
  if (! grub_cln_asset_open (GRUB_CLN_ASSET_CONFIG, config_file))
    {
      errnum = ERR_FILE_NOT_FOUND;
      if (grub_cln_debug)
        grub_printf ("%s(): cannot open GRUB configuration from %s\n", __func__, source);
      return 1;    
    }
  *cfg_file_size = grub_cln_asset_size (GRUB_CLN_ASSET_CONFIG);

  /* Signed/unsigned configuration must fit into the buffer.  */
  if (*cfg_file_size > sizeof (cfg_buffer))
    {
      grub_printf ("configuration file is too big\n");
      errnum = ERR_FILELENGTH;
      return 1;
    }

  /* Buffer configuration file.  */
  read =
    grub_cln_asset_read (GRUB_CLN_ASSET_CONFIG,
                         cfg_buffer
                         + sbh_len,
                         *cfg_file_size);
  grub_cln_asset_close ();
  if (read != *cfg_file_size)
    {
      errnum = ERR_READ;
      if(grub_cln_debug)
        grub_printf ("%s(): cannot read GRUB configuration from %s\n", __func__, source);
      return 1;
    }

  if (grub_cln_secure)
    {
      /* Validate configuration file.  */
      if (! grub_cln_verify_asset_signature ((grub_uint8_t *) cfg_buffer))
        errnum = ERR_CLN_VERIFICATION;

      /* Mask out the CSBH to the configuration file parser.  */
      *cfg_file_buffer += sbh_len;
    }
  return 0;
}
