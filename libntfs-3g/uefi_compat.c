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
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#endif /* __MAKEWITH_GNUEFI */

/*
 * Memory allocation calls that hook into the standard UEFI
 * allocation ones. Note that, in order to be able to use
 * UEFI's ReallocatePool(), we must keep track of the allocated
 * size, which we store as a size_t before the effective payload.
 */
void* malloc(size_t size)
{
	/* Keep track of the allocated size for realloc */
	size_t* ptr = AllocatePool(size + sizeof(size_t));
	if (ptr == NULL)
		return NULL;
	ptr[0] = size;
	return &ptr[1];
}

void* calloc(size_t nmemb, size_t size)
{
	/* Keep track of the allocated size for realloc */
	size_t* ptr = AllocateZeroPool(size * nmemb + sizeof(size_t));
	if (ptr == NULL)
		return NULL;
	ptr[0] = size;
	return &ptr[1];
}

/* NB: As opposed to libc's realloc(), this realloc() always frees the old pointer */
void* realloc(void* p, size_t new_size)
{
	size_t* ptr = (size_t*)p;

	if (ptr == NULL)
		return malloc(new_size);
	/* Access the previous size, which was stored in malloc/calloc */
	ptr = &ptr[-1];
#ifdef __MAKEWITH_GNUEFI
	ptr = ReallocatePool(ptr, (UINTN)*ptr, (UINTN)(new_size + sizeof(size_t)));
#else
	ptr = ReallocatePool((UINTN)*ptr, (UINTN)(new_size + sizeof(size_t)), ptr);
#endif
	if (ptr != NULL)
		*ptr++ = new_size;
	return ptr;
}

void free(void* p)
{
	size_t* ptr = (size_t*)p;
	if (p != NULL)
		FreePool(&ptr[-1]);
}

/*
 * Depending on the compiler, the arch, and the toolchain,
 * these function may or may not need to be provided...
 */
#ifndef USE_COMPILER_INTRINSICS_LIB
#ifndef __MAKEWITH_GNUEFI
#ifndef __arm__
void* memcpy(void* dest, const void* src, size_t n)
{
	CopyMem(dest, src, n);
	return dest;
}
#endif /* __arm__ */

void* memset(void* s, int c, size_t n)
{
	SetMem(s, n, (UINT8)c);
	return s;
}
#endif /* __MAKEWITH_GNUEFI */

void* memmove(void* dest, const void* src, size_t n)
{
	/* CopyMem() supports the handling of overlapped regions */
	CopyMem(dest, src, n);
	return dest;
}

/* Workaround for gcc's 'string_fortified.h: undefined reference to memmove' */
#ifdef __GNUC__
void* __builtin___memmove_chk (void* dest, const void* src, size_t len, size_t destlen)
{
	return memmove(dest, src, len);
}
#endif /* __GNUC__ */

int memcmp(const void* s1, const void* s2, size_t n)
{
	return (int)CompareMem(s1, s2, n);
}
#endif /* USE_COMPILER_INTRINSICS_LIB */

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
