/* uefi_logging.c - UEFI logging */
/*
 *  Copyright © 2014-2026 Pete Batard <pete@akeo.ie>
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

#include "uefi_bridge.h"
#include "uefi_support.h"
#include "uefi_logging.h"
#include "uefi_compat.h"

static UINTN EFIAPI PrintNone(IN CONST CHAR16 *fmt, ... ) { return 0; }
Print_t PrintError    = PrintNone;
Print_t PrintWarning  = PrintNone;
Print_t PrintInfo     = PrintNone;
Print_t PrintVerbose  = PrintNone;
Print_t PrintDebug    = PrintNone;
Print_t* PrintTable[] = { &PrintError, &PrintWarning, &PrintInfo,
		&PrintVerbose, &PrintDebug };

/* Global driver verbosity level */
UINTN LogLevel        = DEFAULT_LOGLEVEL;

UINTN
EFIAPI
PrintDebugger(IN CONST CHAR16* Format, ...)
{
	STATIC CHAR16 UnicodeStr[1024];
	STATIC CHAR8 Utf8Str[1024];
	UINTN Ret = 0;
	VA_LIST Marker;

	/* UnicodeVSPrint() asserts if Format is NULL, so just in case... */
	if (Format == NULL) {
		DEBUG((0x80000000, "ERROR: PrintDebugger() called with NULL Format!\n"));
		return 0;
	}

	VA_START(Marker, Format);
	UnicodeVSPrint(UnicodeStr, sizeof(UnicodeStr), Format, Marker);
	VA_END(Marker);

	Ret = ToUtf8(UnicodeStr, Utf8Str, sizeof(Utf8Str));
	if (Ret > 0)
		DEBUG((0xFFFFFFFF, Utf8Str));
	return Ret;
}

/**
 * Print status
 *
 * @v Status        EFI status code
 */
VOID
PrintStatus(EFI_STATUS Status)
{
	/* Make sure the Status is unsigned 32 bits */
	DEBUG((0xFFFFFFFF, ": [%d] %r\n", (Status & 0x7FFFFFFF), (Status & 0x7FFFFFFF)));
}

/*
 * You can control the verbosity of the driver output by setting the shell environment
 * variable FS_LOGGING to one of the values defined in the FS_LOGLEVEL constants. Or,
 * if that variable is not defined, we set it from PcdDebugPrintErrorLevel (for EDK2).
 */
VOID
SetLogging(VOID)
{
	EFI_GUID ShellVariable = SHELL_VARIABLE_GUID;
	EFI_STATUS Status;
	CHAR16 LogVar[4] = { 0 };
	UINTN i, LogVarSize = sizeof(LogVar);

#if defined(__MAKEWITH_GNUEFI)
	/* gnu-efi's default of ST->StdErr may not always produce output. */
	LibRuntimeDebugOut = ST->ConOut;
#endif

	Status = gRT->GetVariable(L"FS_LOGGING", &ShellVariable, NULL, &LogVarSize, LogVar);
	if (Status == EFI_SUCCESS) {
		LogLevel = FS_LOGLEVEL_NONE;
		/* The log variable should only ever be a single decimal digit */
		if ((LogVar[1] == 0) && (LogVar[0] >= L'0') && (LogVar[0] <= L'9'))
			LogLevel = LogVar[0] - L'0';
	}
#if !defined(__MAKEWITH_GNUEFI)
	else {
		const UINT32 PrintErrorLevel = PcdGet32(PcdDebugPrintErrorLevel);
		if (PrintErrorLevel & DEBUG_ERROR)
			LogLevel = FS_LOGLEVEL_ERROR;
		if (PrintErrorLevel & DEBUG_WARN)
			LogLevel = FS_LOGLEVEL_WARNING;
		if (PrintErrorLevel & DEBUG_INFO)
			LogLevel = FS_LOGLEVEL_INFO;
		if (PrintErrorLevel & DEBUG_VERBOSE)
			LogLevel = FS_LOGLEVEL_VERBOSE;
		if (PrintErrorLevel & DEBUG_FS)
			LogLevel = FS_LOGLEVEL_DEBUG;
		if (PrintErrorLevel & DEBUG_EVENT)
			LogLevel = FS_LOGLEVEL_TRACE;
		if (PrintErrorLevel & DEBUG_INIT)
			LogLevel = FS_LOGLEVEL_ENTER_LEAVE;
	}
#endif

	for (i = 0; i < ARRAYSIZE(PrintTable); i++)
		*PrintTable[i] = (i < LogLevel) ? PrintDebugger : PrintNone;

	NtfsSetLogger(LogLevel);

	PrintVerbose(L"LogLevel = %d\n", LogLevel);
}
