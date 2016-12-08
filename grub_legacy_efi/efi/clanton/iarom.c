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
#include <clanton/iarom.h>
#include <clanton/target.h>
#include <clanton/test_module.h>
#include <shared.h>

/* Structure used to help manage a "heap" for Crypto purposes.
   Note this is a very simple manager with just alloc and init type functions
   there is no free function and so the init function is expected to be called
   between crypto functions.
   Debug and error codes are also stored here so a debugger can access them.  */
typedef struct scratch_memory_t
{
  /* Address of memory from which we can start to allocate.  */
  grub_uint8_t *heap_start;
  /* Address of final byte of memory we can allocate up to.  */
  grub_uint8_t *heap_end;
  /* Pointer to the next free address in the heap.  */
  grub_uint8_t *next_free_mem;
  /* A progress code, updated as we go along.  */
  grub_uint32_t debug_code;
  /* An indicator of why we failed to boot.  */
  grub_uint32_t fatal_code;
}
scratch_memory_t;

static scratch_memory_t *scratch_area_info = NULL;

/* 16 kB of scratchpad memory is sufficient for SHA256/RSA2048.  */
#define CRYPTO_HEAP_SIZE                                     0x4000
static grub_uint8_t memory_buf_array[CRYPTO_HEAP_SIZE];

/* The OEM RSA Public Key for verifying signature.  */
static struct grub_cln_sbh_key_hdr *oem_rsa_key =
  (struct grub_cln_sbh_key_hdr *) (GRUB_CLN_S_KEYMOD_ADDR +
                                   sizeof (struct grub_cln_sbh));

/* Initialise the heap descriptor and the buffer.  */
static void
init_heap (void)
{
  grub_memset (memory_buf_array, 0x0, sizeof (memory_buf_array));

  scratch_area_info = (scratch_memory_t *) &memory_buf_array;
  /* Next address after the structure itself.  */
  scratch_area_info->heap_start = (grub_uint8_t *) (scratch_area_info + 1);
  scratch_area_info->heap_end = (grub_uint8_t *) scratch_area_info + CRYPTO_HEAP_SIZE;
  scratch_area_info->next_free_mem = scratch_area_info->heap_start;
  scratch_area_info->debug_code = 0;
  scratch_area_info->fatal_code = 0;

  if (grub_cln_debug)
    grub_printf ("(%s) scratch_area_info = 0x%x\n", __func__, scratch_area_info);
}

/* The callback function signature.  */
typedef grub_uint8_t (*callback_t) (struct grub_cln_sbh_security_hdr *,
                                    struct grub_cln_sbh_key_hdr *,
                                    struct scratch_memory_t *);
static grub_uint8_t
(*IAROM_validate_module) (struct grub_cln_sbh_security_hdr *sec_h,
                          struct grub_cln_sbh_key_hdr *key_h,
                          struct scratch_memory_t *scratch_area_info);

int
grub_cln_verify_asset_signature (grub_uint8_t *addr)
{
  grub_uint8_t valid = 0;
  struct grub_cln_sbh_security_hdr *sec_h =
      (struct grub_cln_sbh_security_hdr *) addr;
  grub_uint32_t *callback_ptr = (grub_uint32_t *) GRUB_CLN_IAROM_CALLBACK_PTR;

  __cln_test_signature();

  init_heap ();

  IAROM_validate_module = (callback_t) *callback_ptr;

  if (grub_cln_debug)
    {
      grub_printf ("OEM key @ 0x%x\n", oem_rsa_key);
      grub_printf ("Calling into IAROM @ 0x%x for validating module @ 0x%x.. ",
                   IAROM_validate_module, addr);
    }

  valid = IAROM_validate_module (sec_h, oem_rsa_key, scratch_area_info);

  if (grub_cln_debug)
    {
      if (valid)
        grub_printf ("done.");
      else
        grub_printf ("failed.");
      grub_printf (" debug_code=0x%x, fatal_code=0x%x\n",
                   scratch_area_info->debug_code,
                   scratch_area_info->fatal_code);
    }

  return (int) valid;
}
