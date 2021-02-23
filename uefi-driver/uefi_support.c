/* uefi_support.c - UEFI support functions (path, time, etc.) */
/*
 *  Copyright © 2021 Pete Batard <pete@akeo.ie>
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
