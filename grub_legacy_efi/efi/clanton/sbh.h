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

#ifndef GRUB_CLANTON_SBH_HEADER
#define GRUB_CLANTON_SBH_HEADER     1

#include <grub/types.h>

/* Clanton Secure Boot Header.  */

/* Magic number corresponds to "_CSH" in ASCII.  */
#define GRUB_CLN_SBH_MAGIC_NUMBER               0x5F435348

typedef struct grub_cln_sbh
{
  struct grub_cln_sbh_security_hdr
  {
    grub_uint32_t magic_number;                   /* 0x000 */
    grub_uint32_t version;                        /* 0x004 */
    grub_uint32_t module_size;                    /* 0x008 */
    grub_uint32_t svn_index;                      /* 0x00C */
    grub_uint32_t svn;                            /* 0x010 */
    grub_uint32_t module_id;                      /* 0x014 */
    grub_uint32_t module_vendor;                  /* 0x018 */
    grub_uint32_t date;                           /* 0x01C */
    grub_uint32_t header_len;                     /* 0x020 */
    grub_uint32_t hashing_algorithm;              /* 0x024 */
    grub_uint32_t crypto_algorithm;               /* 0x028 */
    grub_uint32_t key_size;                       /* 0x02C */
    grub_uint32_t signature_size;                 /* 0x030 */
    grub_uint32_t next_header_ptr;                /* 0x034 */
    grub_uint8_t  reserved[0x8];                  /* 0x038 */
  } security_header;
  struct grub_cln_csh_key_hdr
  {
    grub_uint32_t key_modulus_size;               /* 0x040 */
    grub_uint32_t key_exponent_size;              /* 0x044 */
    /* Currently we only support RSA. Hence size is hardcoded.  */
    grub_uint32_t key_modulus[256 / sizeof (grub_uint32_t)];    /* 0x048 */
    grub_uint32_t key_exponent[4 / sizeof (grub_uint32_t)];     /* 0x148 */
  } key_structure;
  /* Currently we only support RSA. Hence size is hardcoded.  */
  grub_uint8_t signature[0x100];                  /* 0x14C */
}
*grub_cln_csh_t;


/* Settings for Clanton SBH on filesystem.  */

#define GRUB_CLN_SBH_FILE_EXT                   ".csbh"
/* Be as conservative as possible about the max path length.  Use the minimum
   value allowed by the file systems supported by GRUB.  */
#define GRUB_CLN_SBH_FILE_PATHMAX               1024 /* FIXME  */


#endif /* ! GRUB_CLANTON_SBH_HEADER */
