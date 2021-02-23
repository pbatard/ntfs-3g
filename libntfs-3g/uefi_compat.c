/*
 * uefi_compat.c - Definition of standard function calls for UEFI.
 *
 * Copyright © 2021 Pete Batard <pete@akeo.ie>
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <stdlib.h>

#include "compat.h"
#include "logging.h"
#include "uefi_support.h"

#ifdef __MAKEWITH_GNUEFI

#include <efi.h>
#include <efilib.h>

#else /* __MAKEWITH_GNUEFI */

#include <Base.h>
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#endif /* __MAKEWITH_GNUEFI */

/*
 * Returns the current time in a timespec struct.
 */
int clock_gettime(clockid_t clk_id, struct timespec* now)
{
	EFI_TIME Time;
	EFI_STATUS Status;

	now->tv_sec = 0;
	now->tv_nsec = 0;

	if (clk_id != CLOCK_REALTIME) {
		ntfs_log_perror("%s: Unsupported clock id %d", __FUNCTION__, clk_id);
		errno = ENOSYS;
		return -1;
	}

	Status = gRT->GetTime(&Time, NULL);
	if (EFI_ERROR(Status)) {
		ntfs_log_perror("%s: Failed to get the time (%r)", __FUNCTION__, Status);
		errno = EIO;
		return -1;
	}

	now->tv_sec = EfiTimeToUnixTime(&Time);
	now->tv_nsec = Time.Nanosecond;
	return 0;
}
