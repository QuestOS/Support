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

#include <clanton/perf_metrics.h>
#include <clanton/target.h>
#include <shared.h>

/* String to be appended to the Linux command line.  */
static char perf_metric_string[GRUB_CLN_PERF_METRIC_STRING_MAXLEN] = "";

void
grub_cln_event_reset (void)
{
  grub_memset (perf_metric_string, 0x0, GRUB_CLN_PERF_METRIC_STRING_MAXLEN);
}

void
grub_cln_event_append (const char *tag)
{
  /* Format is (ignore apostrophes): ' tag=0x%016x' plus string terminator,
     with length of tag string limited to GRUB_CLN_BOOT_EVENT_TAG_MAXLEN.  */
  char buf[21 + GRUB_CLN_BOOT_EVENT_TAG_MAXLEN] = "";
  char tag_trimmed[GRUB_CLN_BOOT_EVENT_TAG_MAXLEN]  = "";

  grub_strncpy (tag_trimmed, tag, GRUB_CLN_BOOT_EVENT_TAG_MAXLEN);
  grub_sprintf (buf, " %s=0x%016llx", tag_trimmed, grub_rdtsc ());

  /* Append entry to performance metric string.  */
  grub_strncat (perf_metric_string, buf, GRUB_CLN_PERF_METRIC_STRING_MAXLEN);
}

char *
grub_cln_event_get_metrics (void)
{
  return perf_metric_string;
}
