/* uefi_logging.h - UEFI logging declarations */
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

#ifdef __MAKEWITH_GNUEFI
#include <efi.h>
#else
#include <Base.h>
#include <Uefi.h>
#endif /* __MAKEWITH_GNUEFI */

/* Same as gShellVariableGuid from EDK2 (which gnu-efi doesn't have) */
#define SHELL_VARIABLE_GUID { \
	0x158def5a, 0xf656, 0x419c, { 0xb0, 0x27, 0x7a, 0x31, 0x92, 0xc0, 0x79, 0xd2 } \
}

/* Global log level variable */
extern UINTN LogLevel;

/* Logging */
#define FS_LOGLEVEL_NONE        0
#define FS_LOGLEVEL_ERROR       1
#define FS_LOGLEVEL_WARNING     2
#define FS_LOGLEVEL_INFO        3
#define FS_LOGLEVEL_DEBUG       4
#define FS_LOGLEVEL_EXTRA       5

#if !defined(DEFAULT_LOGLEVEL)
#define DEFAULT_LOGLEVEL        FS_LOGLEVEL_NONE
#endif

/* Print an error message along with a human readable EFI status code */
#define PrintStatusError(Status, Format, ...) \
	do { if (LogLevel >= FS_LOGLEVEL_ERROR) { \
		 Print(Format, ##__VA_ARGS__); PrintStatus(Status); } } while(0)

typedef UINTN(EFIAPI* Print_t)  (IN CONST CHAR16* fmt, ...);
extern Print_t PrintError;
extern Print_t PrintWarning;
extern Print_t PrintInfo;
extern Print_t PrintDebug;
extern Print_t PrintExtra;

extern VOID SetLogging(VOID);
extern VOID PrintStatus(EFI_STATUS Status);
