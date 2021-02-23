/* uefi_support.h - UEFI support declarations */
/*
 *  Copyright © 2014-2021 Pete Batard <pete@akeo.ie>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <time.h>

#include "uefi_driver.h"

#ifndef _MSC_VER
#if !defined(__GNUC__) || (__GNUC__ < 5)
#error gcc 5.0 or later is required for the compilation of this driver.
#endif /* _MSC_VER */

/* Having GNU_EFI_USE_MS_ABI avoids the need for uefi_call_wrapper() */
#if defined(_GNU_EFI) & !defined(GNU_EFI_USE_MS_ABI)
#error gnu-efi, with option GNU_EFI_USE_MS_ABI, is required for the compilation of this driver.
#endif
#endif /* _GNU_EFI & !GNU_EFI_USE_MS_ABI */

#ifndef ARRAYSIZE
#define ARRAYSIZE(A)            (sizeof(A)/sizeof((A)[0]))
#endif

#ifndef MIN
#define MIN(x,y)                ((x)<(y)?(x):(y))
#endif

#ifndef MAX
#define MAX(x,y)                ((x)>(y)?(x):(y))
#endif

#define _STRINGIFY(s)           #s
#define STRINGIFY(s)            _STRINGIFY(s)

#define _WIDEN(s)               L ## s
#define WIDEN(s)                _WIDEN(s)

/* For safety, we set a a maximum size that strings shall not outgrow */
#define STRING_MAX              4098

/* Convenience assertion macros */
#define FL_ASSERT(f, l, a)      if(!(a)) do { Print(L"*** ASSERT FAILED: %a(%d): %a ***\n", f, l, #a); while(1); } while(0)
#define FS_ASSERT(a)            FL_ASSERT(__FILE__, __LINE__, a)

/*
 * Prototypes for the function calls provided in uefi_support.c
 */
VOID UnixTimeToEfiTime(time_t t, EFI_TIME* Time);
time_t EfiTimeToUnixTime(EFI_TIME* Time);
