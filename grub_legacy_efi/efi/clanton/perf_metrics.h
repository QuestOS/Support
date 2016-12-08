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

#ifndef GRUB_CLANTON_TSC_HEADER
#define GRUB_CLANTON_TSC_HEADER     1

#define GRUB_CLN_PERF_METRIC_STRING_MAXLEN 0x400
#define GRUB_CLN_BOOT_EVENT_TAG_MAXLEN 0x15

/* Init/Reinit the performance metric string.  */
void grub_cln_event_reset (void);

/* Log an event along with the current timestamp and append it to the
   performance metric string.
   Tag is limited to GRUB_CLN_BOOT_EVENT_TAG_MAXLEN.
   Performance metric string is limited to GRUB_CLN_PERF_METRIC_STRING_MAXLEN.
   Any string overrun is dropped and no error code/message is thrown.  */
void grub_cln_event_append (const char *tag);

/* Return the performance metric string */
char *grub_cln_event_get_metrics (void);

#endif /* ! GRUB_CLANTON_TSC_HEADER */
