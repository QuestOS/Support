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
#include <grub/types.h>

/* Keep track of error triggering condition.  */
struct __cln_error_trigger_struct {
  int no_kernel;
  int no_ramdisk;
  int no_kernel_sig;
  int no_ramdisk_sig;
  int kernel_sig_fail;
  int ramdisk_sig_fail;
  int bad_imr;
  /* How many signatures have been verified so far.  */
  int state_sign_verify;
};

extern struct __cln_error_trigger_struct __cln_err;

void __cln_test_setup (char *);
void __cln_test_imr (void);

#ifdef INTEL_CLN_TEST

/*
   Failure in fetching an asset results in function returning 0 and errnum
   being set.
 */
#define INTEL_CLN_ERR_NO_ASSET							\
	(! (errnum = ERR_FILE_NOT_FOUND))

#define __cln_test_asset(x)							\
	if (GRUB_CLN_ASSET_KERNEL == x && __cln_err.no_kernel)			\
		return INTEL_CLN_ERR_NO_ASSET;					\
	if (GRUB_CLN_ASSET_INITRD == x && __cln_err.no_ramdisk)			\
		return INTEL_CLN_ERR_NO_ASSET;					\
	if (GRUB_CLN_ASSET_KERNEL_CSBH == x && __cln_err.no_kernel_sig)		\
		return INTEL_CLN_ERR_NO_ASSET;					\
	if (GRUB_CLN_ASSET_INITRD_CSBH == x && __cln_err.no_ramdisk_sig)	\
		return INTEL_CLN_ERR_NO_ASSET;

/*
   Upon failure in verifying signature, IAROM callback only returns 0.
   Possible values for __cln_err.state_sign_verify:
   - 0: we are currently verifying grub.conf
   - 1: we are currently verifying kernel
   - 2: we are currently verifying ramdisk
 */
#define __cln_test_signature(x)							\
	switch (__cln_err.state_sign_verify ++) {				\
	case 0:	/* grub.conf - this is out of scope  */				\
		break;								\
	case 1: /* Kernel  */							\
		if (__cln_err.kernel_sig_fail)					\
			return 0;						\
		break;								\
	case 2: /* Ramdisk  */							\
		if (__cln_err.ramdisk_sig_fail)					\
			return 0;						\
		break;								\
	default:								\
		break;								\
	}

#else /* !INTEL_CLN_TEST  */

#define __cln_test_asset(x)							\
	do {} while(0);
#define __cln_test_signature(x)							\
	do {} while(0);

#endif /* INTEL_CLN_TEST  */

