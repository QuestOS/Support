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

#ifndef CLANTON_TARGET_HEADER
#define CLANTON_TARGET_HEADER

/* Pointer to IAROM callback address for signature verification.  */
#define GRUB_CLN_IAROM_CALLBACK_PTR                            0xFFFFFFE0

/* Read the time-stamp counter.  */
static inline unsigned long long int
grub_rdtsc (void)
{
  unsigned int lo, hi;

  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return (unsigned long long) hi << 32 | lo;
}

#endif /* CLANTON_TARGET_HEADER */
