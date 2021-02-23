/* uefi_driver.c - ntfs-3g UEFI filesystem driver */
/*
 *  Copyright © 2014-2021 Pete Batard <pete@akeo.ie>
 *  Based on iPXE's efi_driver.c and efi_file.c:
 *  Copyright © 2011,2013 Michael Brown <mbrown@fensystems.co.uk>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
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
#include "uefi_logging.h"
#include "uefi_support.h"

#ifdef __MAKEWITH_GNUEFI
/* Designate the driver entrypoint */
EFI_DRIVER_ENTRY_POINT(FSDriverInstall)
#endif /* __MAKEWITH_GNUEFI */

/* We'll try to instantiate a custom protocol as a mutex, so we need a GUID */
EFI_GUID MutexGUID = NTFS_MUTEX_GUID;

/* Keep a global copy of our ImageHanle */
EFI_HANDLE EfiImageHandle = NULL;

/* Handle for our custom protocol/mutex instance */
static EFI_HANDLE MutexHandle = NULL;

/* Keep track of the mounted filesystems */
LIST_ENTRY FsListHead;

/* Custom protocol/mutex definition */
typedef struct {
	INTN Unused;
} EFI_MUTEX_PROTOCOL;
static EFI_MUTEX_PROTOCOL MutexProtocol = { 0 };

static CHAR16* FullDriverName = L"NTFS Driver " WIDEN(STRINGIFY(DRIVER_VERSION))
                                L" (ntfs-3g " WIDEN(STRINGIFY(COMMIT_INFO)) L")";

/* Return the driver name */
static EFI_STATUS EFIAPI
FSGetDriverName(EFI_COMPONENT_NAME_PROTOCOL* This,
	CHAR8* Language, CHAR16** DriverName)
{
	*DriverName = FullDriverName;
	return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
FSGetDriverName2(EFI_COMPONENT_NAME2_PROTOCOL* This,
	CHAR8* Language, CHAR16** DriverName)
{
	*DriverName = FullDriverName;
	return EFI_SUCCESS;
}

/* Return the controller name (unsupported for a filesystem) */
static EFI_STATUS EFIAPI
FSGetControllerName(EFI_COMPONENT_NAME_PROTOCOL* This,
	EFI_HANDLE ControllerHandle, EFI_HANDLE ChildHandle,
	CHAR8* Language, CHAR16** ControllerName)
{
	return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI
FSGetControllerName2(EFI_COMPONENT_NAME2_PROTOCOL* This,
	EFI_HANDLE ControllerHandle, EFI_HANDLE ChildHandle,
	CHAR8* Language, CHAR16** ControllerName)
{
	return EFI_UNSUPPORTED;
}

/*
 * The platform determines whether it will support the older Component
 * Name Protocol or the current Component Name2 Protocol, or both.
 * Because of this, it is strongly recommended that you implement both
 * protocols in your driver.
 *
 * NB: From what I could see, the only difference between Name and Name2
 * is that Name uses ISO-639-2 ("eng") whereas Name2 uses RFC 4646 ("en")
 * See: http://www.loc.gov/standards/iso639-2/faq.html#6
 */
static EFI_COMPONENT_NAME_PROTOCOL FSComponentName = {
	.GetDriverName = FSGetDriverName,
	.GetControllerName = FSGetControllerName,
	.SupportedLanguages = (CHAR8*)"eng"
};

static EFI_COMPONENT_NAME2_PROTOCOL FSComponentName2 = {
	.GetDriverName = FSGetDriverName2,
	.GetControllerName = FSGetControllerName2,
	.SupportedLanguages = (CHAR8*)"en"
};

static EFI_DRIVER_BINDING_PROTOCOL FSDriverBinding = {
	/* This field is used by the EFI boot service ConnectController() to determine the order
	 * that driver's Supported() service will be used when a controller needs to be started.
	 * EFI Driver Binding Protocol instances with higher Version values will be used before
	 * ones with lower Version values. The Version values of 0x0-0x0f and
	 * 0xfffffff0-0xffffffff are reserved for platform/OEM specific drivers. The Version
	 * values of 0x10-0xffffffef are reserved for IHV-developed drivers.
	 */
	.Version = 0x10,
	.ImageHandle = NULL,
	.DriverBindingHandle = NULL
};

/**
 * Uninstall EFI driver
 *
 * @v ImageHandle       Handle identifying the loaded image
 * @ret Status          EFI status code to return on exit
 */
EFI_STATUS EFIAPI
FSDriverUninstall(EFI_HANDLE ImageHandle)
{
	EFI_STATUS Status;
	UINTN NumHandles;
	EFI_HANDLE* Handles = NULL;
	UINTN i;

	/* Enumerate all handles */
	Status = gBS->LocateHandleBuffer(AllHandles, NULL, NULL, &NumHandles, &Handles);

	/* Disconnect controllers linked to our driver. This action will trigger a call to BindingStop */
	if (Status == EFI_SUCCESS) {
		for (i = 0; i < NumHandles; i++) {
			/* Make sure to filter on DriverBindingHandle, else EVERYTHING gets disconnected! */
			Status = gBS->DisconnectController(Handles[i], FSDriverBinding.DriverBindingHandle, NULL);
			if (Status == EFI_SUCCESS)
				PrintDebug(L"DisconnectController[%d]\n", i);
		}
	} else {
		PrintStatusError(Status, L"Unable to enumerate handles");
	}
	if (Handles != NULL)
		gBS->FreePool(Handles);

	/* Now that all controllers are disconnected, we can safely remove our protocols */
	gBS->UninstallMultipleProtocolInterfaces(ImageHandle,
		&gEfiDriverBindingProtocolGuid, &FSDriverBinding,
		&gEfiComponentNameProtocolGuid, &FSComponentName,
		&gEfiComponentName2ProtocolGuid, &FSComponentName2,
		NULL);

	/* Uninstall our mutex (we're the only instance that can run this code) */
	gBS->UninstallMultipleProtocolInterfaces(MutexHandle,
		&MutexGUID, &MutexProtocol,
		NULL);

	PrintDebug(L"FS driver uninstalled.\n");
	return EFI_SUCCESS;
}

/**
 * Install EFI driver - The entrypoint of a driver executable
 *
 * @v ImageHandle       Handle identifying the loaded image
 * @v SystemTable       Pointers to EFI system calls
 * @ret Status          EFI status code to return on exit
 */
EFI_STATUS EFIAPI
FSDriverInstall(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
	EFI_STATUS Status;
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
	VOID* Interface;

#ifdef __MAKEWITH_GNUEFI
	InitializeLib(ImageHandle, SystemTable);
#endif
	SetLogging();
	EfiImageHandle = ImageHandle;

	/* Prevent the driver from being loaded twice by detecting and trying to
	 * instantiate a custom protocol, which we use as a global mutex.
	 */
	Status = gBS->LocateProtocol(&MutexGUID, NULL, &Interface);
	if (Status == EFI_SUCCESS) {
		PrintError(L"This driver has already been installed\n");
		return EFI_LOAD_ERROR;
	}
	/* The only valid status we expect is NOT FOUND here */
	if (Status != EFI_NOT_FOUND) {
		PrintStatusError(Status, L"Could not locate global mutex");
		return Status;
	}
	Status = gBS->InstallMultipleProtocolInterfaces(&MutexHandle,
		&MutexGUID, &MutexProtocol,
		NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not install global mutex");
		return Status;
	}

	/* Grab a handle to this image, so that we can add an unload to our driver */
	Status = gBS->OpenProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid,
		(VOID**)&LoadedImage, ImageHandle,
		NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not open loaded image protocol");
		return Status;
	}

	/* Configure driver binding protocol */
	FSDriverBinding.ImageHandle = ImageHandle;
	FSDriverBinding.DriverBindingHandle = ImageHandle;

	/* Install driver */
	Status = gBS->InstallMultipleProtocolInterfaces(&FSDriverBinding.DriverBindingHandle,
		&gEfiDriverBindingProtocolGuid, &FSDriverBinding,
		&gEfiComponentNameProtocolGuid, &FSComponentName,
		&gEfiComponentName2ProtocolGuid, &FSComponentName2,
		NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not bind driver");
		return Status;
	}

	/* Register the uninstall callback */
	LoadedImage->Unload = FSDriverUninstall;

	PrintDebug(L"FS driver installed.\n");
	return EFI_SUCCESS;
}
