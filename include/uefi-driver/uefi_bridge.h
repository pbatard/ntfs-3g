/* uefi_bridge.h - libntfs-3g interface for UEFI */
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

#pragma once

#include "uefi_driver.h"

/* Used with NtfsGetEfiTime */
#define TIME_CREATED        0
#define TIME_ACCESSED       1
#define TIME_MODIFIED       2

#define NTFS_TO_UNIX_TIME(t)    ((t - (NTFS_TIME_OFFSET)) / 10000000)
#define UNIX_TO_NTFS_TIME(t)    ((t * 10000000) + NTFS_TIME_OFFSET)

/* Same as 'FILE_root' and 'FILE_first_user' from layout.h's NTFS_SYSTEM_FILES */
#define FILE_ROOT           5
#define FILE_FIRST_USER     16

/* Similar to the MREF() macro from libntfs-3g */
#define GetInodeNumber(x)   ((UINT64)((x) & 0XFFFFFFFFFFFFULL))

/* This typedef mirrors the ntfs_filldir_t one in ntfs-3g's dir.h */
typedef INT32(*NTFS_DIRHOOK)(VOID* HookData, CONST CHAR16* Name,
	CONST INT32 NameLen, CONST INT32 NameType, CONST INT64 Pos,
	CONST UINT64 MRef, CONST UINT32 DtType);

VOID NtfsSetErrno(EFI_STATUS Status);
VOID NtfsSetLogger(UINTN LogLevel);
VOID NtfsGetEfiTime(EFI_NTFS_FILE* File, EFI_TIME* Time, INTN Type);
BOOLEAN NtfsIsVolumeReadOnly(VOID* NtfsVolume);
EFI_STATUS NtfsMountVolume(EFI_FS* FileSystem);
EFI_STATUS NtfsUnmountVolume(EFI_FS* FileSystem);
EFI_STATUS NtfsAllocateFile(EFI_NTFS_FILE** File, EFI_FS* FileSystem);
VOID NtfsFreeFile(EFI_NTFS_FILE* File);
EFI_STATUS NtfsOpenFile(EFI_NTFS_FILE** File);
EFI_STATUS NtfsCreateFile(EFI_NTFS_FILE** File);
VOID NtfsCloseFile(EFI_NTFS_FILE* File);
EFI_STATUS NtfsReadDirectory(EFI_NTFS_FILE* File, NTFS_DIRHOOK Hook,
	VOID* HookData);
EFI_STATUS NtfsReadFile(EFI_NTFS_FILE* File, VOID* Data, UINTN* Len);
EFI_STATUS NtfsWriteFile(EFI_NTFS_FILE* File, VOID* Data, UINTN* Len);
EFI_STATUS NtfsGetFileInfo(EFI_NTFS_FILE* File, EFI_FILE_INFO* Info,
	CONST UINT64 MRef, BOOLEAN IsDir);
EFI_STATUS NtfsSetFileInfo(EFI_NTFS_FILE* File, EFI_FILE_INFO* Info);
UINT64 NtfsGetVolumeFreeSpace(VOID* NtfsVolume);
UINT64 NtfsGetFileSize(EFI_NTFS_FILE* File);
EFI_STATUS NtfsRenameVolume(VOID* NtfsVolume, CONST CHAR16* Label,
	CONST INTN Len);
EFI_STATUS NtfsDeleteFile(EFI_NTFS_FILE* File);
EFI_STATUS NtfsFlushFile(EFI_NTFS_FILE* File);
