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
#include <shared.h>

/* The table of builtin commands preserved in Clanton Recovery debug mode.
   Sorted in dictionary order.  */
static char *debug_recovery_cmds[] =
{
  "cat",
  "clear",
  "displaymem",
  "find",
  "geometry",
  "halt",
  "help",
  "read",
  "reboot",
  0
};

/* The table of builtin commands preserved in Clanton Recovery release mode.
   Sorted in dictionary order.  */
static char *release_recovery_cmds[] =
{
  0
};

/* Heap for command line.  */
static char heap[0x80] = "";

/* Perform set intersection between commands in builtin_table[] and cmd_table.  */
static void
restrict_builtin_commands (char *cmd_table[])
{
  struct builtin **builtin = 0;
  char *recovery_cmd = 0;
  unsigned short int keep_cmd = 0;
  unsigned int index = 0;

  for (builtin = builtin_table; *builtin != 0; builtin ++)
    {
      keep_cmd = 0;
      for (index = 0, recovery_cmd = *cmd_table;
           recovery_cmd != 0;
           recovery_cmd = *(++index + cmd_table))
        {
          if (0 == strcmp (recovery_cmd, (*builtin)->name))
            {
              keep_cmd = 1;
              break;
            }
        }

      if (! keep_cmd)
        {
          /* Hide out command and drop the pointer to the routine.  */
          (*builtin)->func = NULL;
          (*builtin)->flags = 0;
        }
    }
}

/* Clanton-specific recovery shell commands definition.  */

static struct builtin builtin_cln_sdio_program =
{
 .name = "cln_sdio_program",
 .func = grub_cln_sdio_program,
 .flags = BUILTIN_CMDLINE | BUILTIN_HELP_LIST,
 .short_doc = "cln_sdio_program",
 .long_doc = "Initiate Clanton SDIO image recovery/installation.",
};

static struct builtin builtin_cln_spi_program =
{
 .name = "cln_fw_recovery",
 .func = grub_cln_spi_program,
 .flags = BUILTIN_CMDLINE | BUILTIN_HELP_LIST,
 .short_doc = "cln_fw_recovery",
 .long_doc = "Perform Firmware-based Clanton system recovery.",
};

static int
add_cln_recovery_command (unsigned short int is_spi_asset)
{
  struct builtin **builtin = 0;
  unsigned short int success = 0;

  /* Make sure that function name and flags are never overwritten.  */
  builtin_cln_sdio_program.func = &grub_cln_sdio_program;
  builtin_cln_sdio_program.flags = BUILTIN_CMDLINE | BUILTIN_HELP_LIST;
  builtin_cln_spi_program.func = &grub_cln_spi_program;
  builtin_cln_spi_program.flags = BUILTIN_CMDLINE | BUILTIN_HELP_LIST;

  /* Place recovery commands into free builtin_table[] entries.
     This should never fail, provided that the original list of built-in
     commands has been already skimmed (e.g. via restrict_builtin_commands).  */
  for (builtin = builtin_table; *builtin != 0; builtin ++)
    {
      if (NULL == (*builtin)->func)
        {
          /* Clanton-specific commands are mutually exclusive.  */
          if (is_spi_asset)
            (*builtin) = &builtin_cln_spi_program;
          else
            (*builtin) = &builtin_cln_sdio_program;

          success = 1;
          break;
        }
    }

  return success;
}

/* Enter a Clanton recovery shell for error handling.
   The shell provides a set of restricted debug commands and Clanton-specific
   commands for Grub-provided SDIO recovery and Firmware-provided SPI recovery.
   The Firmware-provided recovery mechanism is exposed when the failing asset
   resides in SPI/flash. Alternatively the SDIO program utility is made
   available.  */
void
grub_cln_recovery_shell (unsigned short int is_spi_asset)
{
  grub_error_t original_errnum = errnum;

  if (grub_cln_secure)
    {
      /* Restrict available commands.  */
      if (grub_cln_debug)
        restrict_builtin_commands (debug_recovery_cmds); //XXX secure-debug recovery commands..
      else
        restrict_builtin_commands (release_recovery_cmds); //XXX ditto
    }

  /* Add Clanton-specific recovery commands.  */
  if (! add_cln_recovery_command (is_spi_asset))
    grub_printf ("%s() - failed adding Clanton-specific command\n", __func__);

  errnum = original_errnum;

  /* Recovery shell never returns.  But error out noisily if it does.  */
  enter_cmdline (heap, 1, 1);
  grub_printf ("BUG: %s() returning\n", __func__);
}
