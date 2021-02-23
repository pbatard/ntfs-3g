/* uefi_logging.c - UEFI logging */
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

#include "uefi_support.h"
#include "uefi_logging.h"

static UINTN EFIAPI PrintNone(IN CONST CHAR16 *fmt, ... ) { return 0; }
Print_t PrintError    = PrintNone;
Print_t PrintWarning  = PrintNone;
Print_t PrintInfo     = PrintNone;
Print_t PrintDebug    = PrintNone;
Print_t PrintExtra    = PrintNone;
Print_t* PrintTable[] = { &PrintError, &PrintWarning, &PrintInfo,
		&PrintDebug, &PrintExtra };

/* Global driver verbosity level */
UINTN LogLevel = DEFAULT_LOGLEVEL;

/**
 * Print status
 *
 * @v Status        EFI status code
 */
VOID
PrintStatus(EFI_STATUS Status)
{
#if defined(__MAKEWITH_GNUEFI)
	CHAR16 StatusString[64];
	StatusToString(StatusString, Status);
	/* Make sure the Status is unsigned 32 bits */
	Print(L": [%d] %s\n", (Status & 0x7FFFFFFF), StatusString);
#else
	Print(L": [%d]\n", (Status & 0x7FFFFFFF));
#endif
}

/*
 * You can control the verbosity of the driver output by setting the shell environment
 * variable FS_LOGGING to one of the values defined in the FS_LOGLEVEL constants
 */
VOID
SetLogging(VOID)
{
	EFI_GUID ShellVariable = SHELL_VARIABLE_GUID;
	EFI_STATUS Status;
	CHAR16 LogVar[4] = { 0 };
	UINTN i, LogVarSize = sizeof(LogVar);

	Status = gRT->GetVariable(L"FS_LOGGING", &ShellVariable, NULL, &LogVarSize, LogVar);
	if (Status == EFI_SUCCESS) {
		LogLevel = 0;
		/* The log variable should only ever be a single decimal digit */
		if ((LogVar[1] == 0) && (LogVar[0] >= L'0') && (LogVar[0] <= L'9'))
			LogLevel = LogVar[0] - L'0';
	}

	for (i = 0; i < ARRAYSIZE(PrintTable); i++)
		*PrintTable[i] = (i < LogLevel)?(Print_t)Print:(Print_t)PrintNone;

	/* TODO: Call into ntfs_log_set_levels() */

	PrintExtra(L"LogLevel = %d\n", LogLevel);
}
