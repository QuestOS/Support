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

#include <clanton/clanton.h>
#include <clanton/flash.h>
#include <clanton/mfh.h>
#include <shared.h>

#define offsetof(st, m) \
  ((unsigned int) ( (char *)&((st *)0)->m - (char *)0 ))

static struct grub_cln_mfh cln_mfh;
static int cln_mfh_loaded = 0;

void
grub_cln_mfh_load (const struct grub_cln_mfh *mfh)
{
  grub_memcpy (&cln_mfh, (grub_uint8_t *) mfh, sizeof (cln_mfh));
  cln_mfh_loaded = 1;
}

grub_error_t
grub_cln_mfh_entry_lookup (const grub_uint8_t *mfh_addr, unsigned int entry_type,
                           grub_uint8_t **addr, grub_uint32_t *len)
{
  unsigned int offset = 0x0;
  unsigned int i = 0;
  grub_cln_mfh_item_t item = 0x0;

  if (! mfh_addr || ! addr || ! len)
    {
      grub_printf ("%s: NULL pointer\n", __func__);
      errnum = ERR_BAD_ARGUMENT;
      return errnum;
    }

  /* Fetch the MFH if not done yet.  */
  if (! cln_mfh_loaded)
    grub_cln_mfh_load ((grub_cln_mfh_t) mfh_addr);

  /* Sanity check.  */
  if (GRUB_CLN_MFH_IDENTIFIER != cln_mfh.identifier)
    {
      cln_mfh_loaded = 0;
      grub_printf ("%s: invalid MFH identifier\n", __func__);
      errnum = ERR_EXEC_FORMAT;
      return errnum;
    }

  /* Look up.  */
  offset = offsetof (struct grub_cln_mfh, padding)
           + sizeof (grub_uint32_t) * cln_mfh.boot_prio_list_count;
  for (i = 0; i < cln_mfh.flash_item_count; i ++, offset += sizeof (*item))
    {
      item = (grub_cln_mfh_item_t) ((grub_uint8_t *) &cln_mfh + offset);
      if (item->type == entry_type)
        {
          *len = item->flash_item_len;
          *addr = (grub_uint8_t *) item->flash_item_addr;
          if (grub_cln_debug)
            grub_printf ("%s: found entry 0x%x @addr=0x%x, len=0x%x\n",
                         __func__, item->type, *addr, *len);
          return errnum;
        }
    }

  /* At this stage, we haven't found the item.  */
  if (grub_cln_debug)
    {
      grub_printf ("%s: flash item 0x%x not found\n", __func__, entry_type);
    }
  errnum = ERR_FILE_NOT_FOUND;
  return errnum;
}
