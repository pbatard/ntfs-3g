/* uefi_file.c - SimpleFileIo Interface */
/*
 *  Copyright © 2014-2021 Pete Batard <pete@akeo.ie>
 *  Based on iPXE's efi_driver.c and efi_file.c:
 *  Copyright © 2011,2013 Michael Brown <mbrown@fensystems.co.uk>.
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

#include "uefi_driver.h"
#include "uefi_bridge.h"
#include "uefi_logging.h"
#include "uefi_support.h"

/**
 * Open file
 *
 * @v This			File handle
 * @ret new			New file handle
 * @v Name			File name
 * @v Mode			File mode
 * @v Attributes	File attributes (for newly-created files)
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileOpen(EFI_FILE_HANDLE This, EFI_FILE_HANDLE* New,
	CHAR16* Name, UINT64 Mode, UINT64 Attributes)
{
	return EFI_UNSUPPORTED;
}

/* Ex version */
EFI_STATUS EFIAPI
FileOpenEx(EFI_FILE_HANDLE This, EFI_FILE_HANDLE* New, CHAR16* Name,
	UINT64 Mode, UINT64 Attributes, EFI_FILE_IO_TOKEN* Token)
{
	return EFI_UNSUPPORTED;
}

/**
 * Close file
 *
 * @v This			File handle
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileClose(EFI_FILE_HANDLE This)
{
	return EFI_UNSUPPORTED;
}

/**
 * Close and delete file
 *
 * @v This			File handle
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileDelete(EFI_FILE_HANDLE This)
{
	return EFI_UNSUPPORTED;
}

/**
 * Read from file
 *
 * @v This			File handle
 * @v Len			Length to read
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileRead(EFI_FILE_HANDLE This, UINTN* Len, VOID* Data)
{
	return EFI_UNSUPPORTED;
}

/* Ex version */
EFI_STATUS EFIAPI
FileReadEx(IN EFI_FILE_PROTOCOL *This, IN OUT EFI_FILE_IO_TOKEN *Token)
{
	return EFI_UNSUPPORTED;
}

/**
 * Write to file
 *
 * @v This			File handle
 * @v Len			Length to write
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileWrite(EFI_FILE_HANDLE This, UINTN* Len, VOID* Data)
{
	return EFI_UNSUPPORTED;
}

/* Ex version */
EFI_STATUS EFIAPI
FileWriteEx(IN EFI_FILE_PROTOCOL* This, EFI_FILE_IO_TOKEN* Token)
{
	return EFI_UNSUPPORTED;
}

/**
 * Set file position
 *
 * @v This			File handle
 * @v Position		New file position
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileSetPosition(EFI_FILE_HANDLE This, UINT64 Position)
{
	return EFI_UNSUPPORTED;
}

/**
 * Get file position
 *
 * @v This			File handle
 * @ret Position	New file position
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileGetPosition(EFI_FILE_HANDLE This, UINT64* Position)
{
	return EFI_UNSUPPORTED;
}

/**
 * Get file information
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileGetInfo(EFI_FILE_HANDLE This, EFI_GUID* Type, UINTN* Len, VOID* Data)
{
	return EFI_UNSUPPORTED;
}

/**
 * Set file information
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileSetInfo(EFI_FILE_HANDLE This, EFI_GUID* Type, UINTN Len, VOID* Data)
{
	return EFI_UNSUPPORTED;
}

/**
 * Flush file modified data
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileFlush(EFI_FILE_HANDLE This)
{
	return EFI_UNSUPPORTED;
}

/* Ex version */
EFI_STATUS EFIAPI
FileFlushEx(EFI_FILE_HANDLE This, EFI_FILE_IO_TOKEN* Token)
{
	return EFI_UNSUPPORTED;
}

/**
 * Open the volume and return a handle to the root directory
 *
 * @v This			EFI simple file system
 * @ret Root		File handle for the root directory
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileOpenVolume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* This, EFI_FILE_HANDLE* Root)
{
	EFI_STATUS Status;
	EFI_NTFS_FILE* RootFile = NULL;
	EFI_FS* FSInstance = BASE_CR(This, EFI_FS, FileIoInterface);

	PrintInfo(L"OpenVolume: %s\n", FSInstance->DevicePathString);

	/* TODO: Mount the NTFS volume */

	/* Create the root file */
	Status = NtfsAllocateFile(&RootFile, FSInstance);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not create root file");
		goto out;
	}

	/* TODO: Open the root file */
	RootFile->IsRoot = TRUE;
	RootFile->IsDir = TRUE;

	/* Setup the path */
	RootFile->Path = AllocateZeroPool(2 * sizeof(CHAR16));
	if (RootFile->Path == NULL) {
		Status = EFI_OUT_OF_RESOURCES;
		PrintStatusError(Status, L"Could not allocate root file name");
		goto out;
	}
	RootFile->Path[0] = PATH_CHAR;
	RootFile->BaseName = &RootFile->Path[1];

	/* Set the inital refcount */
	RootFile->RefCount = 1;

	/* Return the root handle */
	*Root = (EFI_FILE_HANDLE)RootFile;
	Status = EFI_SUCCESS;

out:
	if (EFI_ERROR(Status))
		NtfsFreeFile(RootFile);
	return Status;
}

/**
 * Install the EFI simple file system protocol
 * If successful this call instantiates a new FS#: drive, that is made
 * available on the next 'map -r'. Note that all this call does is add
 * the FS protocol. OpenVolume won't be called until a process tries
 * to access a file or the root directory on the volume.
 */
EFI_STATUS
FSInstall(EFI_FS* This, EFI_HANDLE ControllerHandle)
{
	const CHAR8 NtfsMagic[8] = { 'N', 'T', 'F', 'S', ' ', ' ', ' ', ' ' };
	EFI_STATUS Status;
	CHAR8* Buffer;

	/*
	 * Check if it's a filesystem we can handle by reading the first block
	 * of the volume and looking for the NTFS magic in the OEM ID.
	 */
	Buffer = (CHAR8*)AllocateZeroPool(This->BlockIo->Media->BlockSize);
	if (Buffer == NULL)
		return EFI_OUT_OF_RESOURCES;
	Status = This->BlockIo->ReadBlocks(This->BlockIo, This->BlockIo->Media->MediaId,
			0, This->BlockIo->Media->BlockSize, Buffer);
	if (!EFI_ERROR(Status))
		Status = CompareMem(&Buffer[3], NtfsMagic, sizeof(NtfsMagic)) ? EFI_UNSUPPORTED : EFI_SUCCESS;
	FreePool(Buffer);
	if (EFI_ERROR(Status))
		return Status;

	PrintInfo(L"FSInstall: %s\n", This->DevicePathString);

	/* Install the simple file system protocol. */
	Status = gBS->InstallMultipleProtocolInterfaces(&ControllerHandle,
		&gEfiSimpleFileSystemProtocolGuid, &This->FileIoInterface,
		NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not install simple file system protocol");
		return Status;
	}

	InitializeListHead(&FsListHead);

	return EFI_SUCCESS;
}

/* Uninstall EFI simple file system protocol */
VOID
FSUninstall(EFI_FS* This, EFI_HANDLE ControllerHandle)
{
	PrintInfo(L"FSUninstall: %s\n", This->DevicePathString);

	gBS->UninstallMultipleProtocolInterfaces(ControllerHandle,
		&gEfiSimpleFileSystemProtocolGuid, &This->FileIoInterface,
		NULL);
}
