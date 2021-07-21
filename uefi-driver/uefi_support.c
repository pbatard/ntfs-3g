/* uefi_support.c - UEFI support functions (path, time, etc.) */
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

/*
 * Convert a GUID to a Unicode string.
 * NB: Concurrent accesses to this call are not supported.
 */
CHAR16* GuidToStr(EFI_GUID* Guid)
{
	static CHAR16 GuidStr[37];
	UnicodeSPrint(GuidStr, ARRAYSIZE(GuidStr),
		L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		Guid->Data1, Guid->Data2, Guid->Data3, Guid->Data4[0],
		Guid->Data4[1], Guid->Data4[2], Guid->Data4[3], Guid->Data4[4],
		Guid->Data4[5], Guid->Data4[6], Guid->Data4[7]);
	return GuidStr;
}

/*
 * Convert Unix time to EFI time. Mostly taken from:
 * https://www.oryx-embedded.com/doc/date__time_8c_source.html
 */
VOID UnixTimeToEfiTime(time_t t, EFI_TIME* Time)
{
	UINT32 a, b, c, d, e, f;

	ZeroMem(Time, sizeof(EFI_TIME));

	/* Negative Unix time values are not supported */
	if (t < 1)
		return;

	/* Clear nanoseconds */
	Time->Nanosecond = 0;

	/* Retrieve hours, minutes and seconds */
	Time->Second = t % 60;
	t /= 60;
	Time->Minute = t % 60;
	t /= 60;
	Time->Hour = t % 24;
	t /= 24;

	/* Convert Unix time to date */
	a = (UINT32)((4 * t + 102032) / 146097 + 15);
	b = (UINT32)(t + 2442113 + a - (a / 4));
	c = (20 * b - 2442) / 7305;
	d = b - 365 * c - (c / 4);
	e = d * 1000 / 30601;
	f = d - e * 30 - e * 601 / 1000;

	/* January and February are counted as months 13 and 14 of the previous year */
	if (e <= 13) {
		c -= 4716;
		e -= 1;
	} else {
		c -= 4715;
		e -= 13;
	}

	/* Retrieve year, month and day */
	Time->Year = c;
	Time->Month = e;
	Time->Day = f;
}

/*
 * Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 *
 * Derived from the GPLv2+ implementation found at:
 * http://xenbits.xen.org/hg/xen-3.3-testing.hg/file/98fe9e75d24b/extras/mini-os/arch/ia64/time.c
 */
time_t EfiTimeToUnixTime(EFI_TIME* Time)
{
	UINTN Month = Time->Month, Year = Time->Year;

	/* 1..12 -> 11,12,1..10 */
	if (0 >= (INTN)(Month -= 2)) {
		Month += 12;	/* Puts Feb last since it has leap day */
		Year -= 1;
	}

	return (
		(
			((UINT64)
				(Year / 4 - Year / 100 + Year / 400 + 367 * Month / 12 + Time->Day) +
				Year * 365 - 719499
				) * 24 + Time->Hour /* now have hours */
			) * 60 + Time->Minute /* now have minutes */
		) * 60 + Time->Second; /* finally seconds */
}

/*
 * Poor man's Device Path to string conversion, where we
 * simply convert the path buffer to hexascii.
 * This is needed to support old Dell UEFI platforms, that
 * don't have the Device Path to Text protocol...
 */
CHAR16*
DevicePathToHex(CONST EFI_DEVICE_PATH* DevicePath)
{
	UINTN Len = 0, i;
	CHAR16* DevicePathString = NULL;
	UINT8* Dp = (UINT8*)DevicePath;

	if (DevicePath == NULL)
		return NULL;

	while (!IsDevicePathEnd(DevicePath)) {
		UINT16 NodeLen = DevicePathNodeLength(DevicePath);
		// Device Paths are provided by UEFI and are sanitized.
		// coverity[var_assign_var]
		Len += NodeLen;
		DevicePath = (EFI_DEVICE_PATH*)((UINT8*)DevicePath + NodeLen);
	}

	DevicePathString = AllocatePool((2 * Len + 1) * sizeof(CHAR16));
	for (i = 0; i < Len; i++) {
		DevicePathString[2 * i] = ((Dp[i] >> 4) < 10) ?
			((Dp[i] >> 4) + '0') : ((Dp[i] >> 4) - 0xa + 'A');
		DevicePathString[2 * i + 1] = ((Dp[i] & 15) < 10) ?
			((Dp[i] & 15) + '0') : ((Dp[i] & 15) - 0xa + 'A');
	}
	DevicePathString[2 * Len] = 0;

	return DevicePathString;
}

/*
 * Convert a Device Path to a string.
 * The returned value Must be freed with FreePool().
 */
CHAR16*
DevicePathToString(CONST EFI_DEVICE_PATH* DevicePath)
{
	CHAR16* DevicePathString = NULL;
	EFI_STATUS Status;
	EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* DevicePathToText;

	Status = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, (VOID**)&DevicePathToText);
	if (Status == EFI_SUCCESS)
		DevicePathString = DevicePathToText->ConvertDevicePathToText(DevicePath, FALSE, FALSE);
	else
#if defined(_GNU_EFI)
		DevicePathString = DevicePathToStr((EFI_DEVICE_PATH*)DevicePath);
#else
		DevicePathString = DevicePathToHex(DevicePath);
#endif
	return DevicePathString;
}

/*
 * Compare two device path instances.
 * Returns 0 if the two instances match.
 */
INTN
CompareDevicePaths(CONST EFI_DEVICE_PATH* dp1, CONST EFI_DEVICE_PATH* dp2)
{
	if (dp1 == NULL || dp2 == NULL)
		return -1;

	while (1) {
		UINT8 type1, type2;
		UINT8 subtype1, subtype2;
		UINT16 len1, len2;
		INTN ret;

		type1 = DevicePathType(dp1);
		type2 = DevicePathType(dp2);

		if (type1 != type2)
			return (INTN)type2 - (INTN)type1;

		subtype1 = DevicePathSubType(dp1);
		subtype2 = DevicePathSubType(dp2);

		if (subtype1 != subtype2)
			return (INTN)subtype1 - (INTN)subtype2;

		len1 = DevicePathNodeLength(dp1);
		len2 = DevicePathNodeLength(dp2);
		if (len1 != len2)
			return (INTN)len1 - (INTN)len2;

		ret = CompareMem(dp1, dp2, len1);
		if (ret != 0)
			return ret;

		if (IsDevicePathEnd(dp1))
			break;

		dp1 = (EFI_DEVICE_PATH*)((UINT8*)dp1 + len1);
		dp2 = (EFI_DEVICE_PATH*)((UINT8*)dp2 + len2);
	}

	return 0;
}

/* "The value 0xFFFF is guaranteed not to be a Unicode character at all" */
#define BLANK_CHAR 0xFFFF

/*
 * Clean an existing path from '.', '..' and/or double path separators.
 * This will for instance tranform 'a//b/c/./..///d/./e/..' into 'a/b/d'.
 * Note that, for anything but '/', trailing path separtors are removed.
 */
VOID CleanPath(CHAR16* Path)
{
	UINTN i, j, SepCount = 1;
	CHAR16* p, ** SepPos;

	if (!Path || !*Path)
		return;

	/* Count the number of path separators */
	for (p = Path; *p; p++)
		if (*p == PATH_CHAR)
			SepCount++;

	/* Allocate an array of pointers large enough */
	SepPos = AllocatePool((SepCount + 1) * sizeof(CHAR16*));
	if (SepPos == NULL)
		return;

	/* Fill our array with pointers to the path separators */
	for (i = 1, p = Path; *p; p++)
		if (*p == PATH_CHAR)
			SepPos[i++] = p;

	/*
	 * To simplify processing, set Path[-1] and
	 * Path[StrLen(Path)] as path separators.
	 */
	SepPos[0] = &Path[-1];
	SepPos[SepCount++] = p;

	/* Now eliminate every single '.' */
	for (i = 1; i < SepCount; i++) {
		/*
		 * Be weary that, per C89 and subsequent C standards:
		 * "When two pointers to members of the same array object are
		 *  subtracted, the difference is divided by the size of a member."
		 * This means that we don't need to correct for sizeof(CHAR16) below.
		 */
		if (SepPos[i] - SepPos[i - 1] == 2 && SepPos[i - 1][1] == L'.') {
			/* Remove the whole section including the leading path sep */
			SepPos[i - 1][1] = BLANK_CHAR;
			if (i > 1)
				SepPos[i - 1][0] = BLANK_CHAR;
		}
	}

	/* And remove sections preceding '..' */
	for (i = 1; i < SepCount; i++) {
		if (SepPos[i] - SepPos[i - 1] == 3 && SepPos[i - 1][1] == L'.' && SepPos[i - 1][2] == L'.') {
			j = i >= 2 ? i - 2 : 0;
			/*
			 * Look for a previous section start, taking care to
			 * step over the ones that have already been eliminated
			 */
			while (j > 0 && *SepPos[j] == BLANK_CHAR)
				j--;
			for (p = (j > 0 ? SepPos[j] : Path); p < SepPos[i]; p++)
				*p = BLANK_CHAR;
		}
	}

	/* Finally, rewrite the path eliminating BLANK_CHAR and double PATH_CHAR */
	for (i = 0, p = Path; *p; p++)
		if (*p != BLANK_CHAR && (*p != PATH_CHAR || i == 0 || Path[i - 1] != PATH_CHAR))
			Path[i++] = *p;

	/* Make sure we don't end up with an empty string */
	if (i == 0)
		Path[i++] = PATH_CHAR;
	Path[i] = 0;

	/* Remove any trailing PATH_CHAR (but keep '/' as is) */
	if (i > 1 && Path[i - 1] == PATH_CHAR)
		Path[i - 1] = 0;
}
