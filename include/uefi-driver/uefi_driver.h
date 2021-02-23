/* uefi_driver.h - ntfs-3g UEFI filesystem driver */
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

#ifdef __MAKEWITH_GNUEFI

#include <efi.h>
#include <efilib.h>
#include <efidebug.h>

#else /* __MAKEWITH_GNUEFI */

#include <Base.h>
#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/BlockIo2.h>
#include <Protocol/DiskIo.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>

#include <Guid/FileSystemInfo.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Guid/ShellVariableGuid.h>

#endif /* __MAKEWITH_GNUEFI */

#define NTFS_MUTEX_GUID { 0xf4ed18ca, 0xcdfb, 0x40ca, { 0x97, 0xec, 0x32, 0x2a, 0x8b, 0x01, 0x4e, 0x5f } }

/* Version information to be displayed by the driver. */
#ifndef DRIVER_VERSION
#define DRIVER_VERSION              DEV
#endif
#ifndef COMMIT_INFO
#define COMMIT_INFO                 unknown
#endif
