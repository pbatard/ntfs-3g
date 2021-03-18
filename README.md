ntfs-3g UEFI NTFS driver
========================

[![Build status](https://img.shields.io/github/workflow/status/pbatard/ntfs-3g/UEFI%20driver%20-%20Linux%20(gcc%20with%20EDK2)%20build.svg?style=flat-square)](https://github.com/pbatard/ntfs-3g/actions)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/23353.svg?style=flat-square)](https://scan.coverity.com/projects/pbatard-ntfs-3g)
[![Release](https://img.shields.io/github/release-pre/pbatard/ntfs-3g.svg?style=flat-square)](https://github.com/pbatard/ntfs-3g/releases)
[![Github stats](https://img.shields.io/github/downloads/pbatard/ntfs-3g/total.svg?style=flat-square)](https://github.com/pbatard/ntfs-3g/releases)
[![Licence](https://img.shields.io/badge/license-GPLv2-blue.svg?style=flat-square)](https://www.gnu.org/licenses/gpl-2.0.en.html)

## DESCRIPTION

This repository is a fork of the [ntfs-3g project](https://github.com/tuxera/ntfs-3g),
that enables building ntfs-3g into a fully functional UEFI NTFS driver.

The resulting driver, which includes both read and write capabilities, can be
compiled with either Visual Studio 2019 (EDK2 or gnu-efi) or gcc (EDK2 only)
for all of the IA32, X64, ARM and AARCH64 UEFI architectures.

If using [Visual Studio 2019](https://visualstudio.microsoft.com/vs/), the
driver can also be tested through [QEMU](https://www.qemu.org/).

The changes that are applied on top of the ntfs-3g source can be found, as
detailed individual commits, in this repository.

## COMPILATION

### General options

The default UEFI driver provides read-write access to an NTFS volume.

If you would rather compile a read-only version of this driver, you can
either define the `FORCE_READONLY` macro in `include/uefi-driver/uefi-driver.h`
or pass that macro as a compiler option (through '-D FORCE_READONLY=TRUE' if
using EDK2).

### Linux ([EDK2](https://github.com/tianocore/edk2))

This assumes that you have gcc (5.0 or later) and the EDK2 installed.
For this example, we assume that EDK2 is located in `/usr/src/edk2`.

Open a command prompt at the top of this repository and issue:

```
export WORKSPACE=$PWD
export EDK2_PATH=/usr/src/edk2
export PACKAGES_PATH=$WORKSPACE:$EDK2_PATH
source $EDK2_PATH/edksetup.sh --reconfig
build -a X64 -b RELEASE -t GCC5 -p uefi-driver.dsc
```

You may also use `IA32`, `ARM`, `AARCH64` or `RISCV64` for the `-a` parameter to
build a driver for these architectures noting however that you need to have the
relevant toolchain and GCC prefix set.

For instance, to compile for ARM, ARM64 and RISCV64, you would need to also issue:

```
export GCC5_ARM_PREFIX=arm-linux-gnueabi-
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
export GCC5_RISCV64_PREFIX=riscv64-linux-gnu-
```

### Windows ([gnu-efi](https://sourceforge.net/p/gnu-efi/code/ci/master/tree/))

This assumes that you have Visual Studio 2019 installed.

Open the VS2019 solution file and build using the IDE.

If you have QEMU installed under `C:\Program Files\qemu\` you should also be
able to test the driver for any of the supported architectures (IA32, X64, ARM
and ARM64). Of course building and testing for the latter requires that you
selected the relevant C++ components during Visual Studio installation. Any
required test components (NTFS virtual disk, QEMU UEFI firmware) is downloaded
automatically by the test script.

### Windows ([EDK2](https://github.com/tianocore/edk2))

This assumes that you have Visual Studio 2019 and the EDK2 installed.
For this examples, we assume that EDK2 is installed in `D:\EDK2`.

Open a command prompt at the top of this repository and issue:

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
set WORKSPACE=%cd%
set EDK2_PATH=D:\EDK2
set PACKAGES_PATH=%WORKSPACE%;%EDK2_PATH%
%EDK2_PATH%\edksetup.bat reconfig
build -a X64 -b RELEASE -t VS2019 -p uefi-driver.dsc
```

You may also use `IA32`, `ARM` or `AARCH64` for the `-a` parameter to build a
driver for these architectures noting however that, for ARM or ARM64 compilation,
you must have selected the relevant C++ components during Visual Studio install.

## USAGE

Simply load the driver from a UEFI Shell using the `load` command and then
issue `map -r` to remap volumes. This should grant access to any NTFS volume
that is accessible from UEFI.

Note that NTFS volumes are unmounted, fully, as soon as the last open file
handle to that volume is closed. This should make the driver safe for surprise
removal/shutdown as long as you don't perform these operations in the middle of
a Shell command or during the execution of a UEFI application.

## COMPLIANCE

This UEFI NTFS driver should comply with the UEFI specifications, and
especially with sections 13.4 and 13.5 from 
[version 2.8 Errata A of the specs](https://uefi.org/sites/default/files/resources/UEFI_Spec_2_8_A_Feb14.pdf).
