## @file
#  NTFS Driver Module
#
#  Copyright (c) 2021, Pete Batard <pete@akeo.ie>
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#
##

[Defines]
  PLATFORM_NAME                  = ntfs-3g
  PLATFORM_GUID                  = 6DF51CCF-47D8-4035-8F1B-049B72C6DECF
  PLATFORM_VERSION               = 1.3
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = IA32|X64|EBC|ARM|AARCH64|RISCV64
  OUTPUT_DIRECTORY               = Build
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  DEFINE FORCE_READONLY          = FALSE

[BuildOptions]
  DEBUG_*_*_CC_FLAGS             = -DENABLE_DEBUG
  RELEASE_*_*_CC_FLAGS           = -DMDEPKG_NDEBUG
  *_*_*_CC_FLAGS                 = -DDISABLE_NEW_DEPRECATED_INTERFACES
!ifdef DRIVER_VERSION
  *_*_*_CC_FLAGS                 = -DDRIVER_VERSION=$(DRIVER_VERSION)
!endif
!ifdef COMMIT_INFO
  *_*_*_CC_FLAGS                 = -DCOMMIT_INFO=$(COMMIT_INFO)
!endif
!if $(FORCE_READONLY) == TRUE
  *_*_*_CC_FLAGS                 = -DFORCE_READONLY
!endif

!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses]
  #
  # Entry Point Libraries
  #
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  #
  # Common Libraries
  #
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf  
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf

[LibraryClasses.ARM, LibraryClasses.AARCH64, LibraryClasses.RISCV64]
  NULL|ArmPkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
  NULL|MdePkg/Library/BaseStackCheckLib/BaseStackCheckLib.inf

[LibraryClasses.IA32, LibraryClasses.X64]
!if $(TOOLCHAIN) == "VS2019"
  NULL|.vs/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
!endif

###################################################################################################
#
# Components Section - list of the modules and components that will be processed by compilation
#                      tools and the EDK II tools to generate PE32/PE32+/Coff image files.
#
###################################################################################################

[Components]
  uefi-driver/uefi_driver.inf
