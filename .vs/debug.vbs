' Visual Studio QEMU debugging script.
' Copyright © 2014-2021 Pete Batard <pete@akeo.ie>
'
' This program is free software: you can redistribute it and/or modify
' it under the terms of the GNU General Public License as published by
' the Free Software Foundation, either version 2 of the License, or
' (at your option) any later version.
'
' This program is distributed in the hope that it will be useful,
' but WITHOUT ANY WARRANTY; without even the implied warranty of
' MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
' GNU General Public License for more details.
'
' You should have received a copy of the GNU General Public License
' along with this program.  If not, see <http://www.gnu.org/licenses/>.
'
' I like invoking vbs as much as anyone else, but we need to download and unzip a
' bunch of files, as well as launch QEMU, and neither Powershell or a standard batch
' can do that without having an extra console appearing.
'
' Note: You may get a prompt from the Windows firewall when trying to download files

' Modify these variables as needed
QEMU_PATH  = "C:\Program Files\qemu\"
' You can add something like "-S -gdb tcp:127.0.0.1:1234" if you plan to use gdb to debug
' You can also use '-serial file:serial.log' instead of '-serial vc' to send output to a file
QEMU_OPTS  = "-nodefaults -vga std -serial vc"
' Set to True if you don't want to execute the bootloader
LIST_ONLY  = True
' Set to True if you need to download a file that might be cached locally
NO_CACHE   = False
' Set to True if you want to use drivers build through EDK2 instead of the VS/gnu-efi ones
USE_EDK2   = False

' You shouldn't have to mofify anything below this
CONF       = WScript.Arguments(0)
FS         = WScript.Arguments(1)
BIN        = WScript.Arguments(2)
TARGET     = WScript.Arguments(3)

If (TARGET = "x86") Then
  UEFI_EXT  = "ia32"
  QEMU_ARCH = "i386"
  FW_BASE   = "OVMF"
  EDK_ARCH  = "IA32"
ElseIf (TARGET = "x64") Then
  UEFI_EXT  = "x64"
  QEMU_ARCH = "x86_64"
  FW_BASE   = "OVMF"
  EDK_ARCH  = "X64"
ElseIf (TARGET = "ARM") Then
  UEFI_EXT  = "arm"
  QEMU_ARCH = "arm"
  ' You can also add '-device VGA' to the options below, to get graphics output.
  ' But if you do, be mindful that the keyboard input may not work... :(
  QEMU_OPTS = "-M virt -cpu cortex-a15 " & QEMU_OPTS
  FW_BASE   = "QEMU_EFI"
  EDK_ARCH  = "ARM"
ElseIf (TARGET = "ARM64") Then
  UEFI_EXT  = "aa64"
  QEMU_ARCH = "aarch64"
  QEMU_OPTS = "-M virt -cpu cortex-a57 " & QEMU_OPTS
  FW_BASE   = "QEMU_EFI"
  EDK_ARCH  = "AARCH64"
Else
' MSVC does not support RISC-V yet, but EDK2 does. To test the RISC-V EDK2 driver you can:
' 1. Download the RISCVVIRT.fd QEMU firmware such as the one found in the artifacts from:
'    https://github.com/riscv/riscv-edk2-platforms/actions/runs/885101747
' 2. Create a `disk.img` with 2 partition (FAT32 + NTFS) and copy the NTFS driver to the
'    FAT32 one. Note that you can't currently use separate disk images with Risc-V QEMU.
' 3. Run: "C:\Program Files\qemu\qemu-system-riscv64w.exe" -machine virt -nodefaults -vga std -serial vc -L . -bios RISCVVIRT.fd -m 2048 -smp cpus=1,maxcpus=1 -device virtio-blk-device,drive=hd0 -drive file=disk.img,format=raw,id=hd0
  MsgBox("Unsupported debug target: " & TARGET)
  Call WScript.Quit(1)
End If
BOOT_NAME  = "boot" & UEFI_EXT & ".efi"
FW_ARCH    = UCase(UEFI_EXT)
FW_DIR     = "https://efi.akeo.ie/" & FW_BASE & "/"
FW_ZIP     = FW_BASE & "-" & FW_ARCH & ".zip"
FW_FILE    = FW_BASE & "_" & FW_ARCH & ".fd"
FW_URL     = FW_DIR & FW_ZIP
QEMU_EXE   = "qemu-system-" & QEMU_ARCH & "w.exe"

LOG_LEVEL  = 0
If (CONF = "Debug") Then
  LOG_LEVEL = 4
End If
IMG_EXT    = ".vhd"
IMG        = FS & IMG_EXT
IMG_ZIP    = FS & ".zip"
IMG_URL    = "https://efi.akeo.ie/test/" & IMG_ZIP
DRV        = FS & "_" & UEFI_EXT & ".efi"
MNT        = "fs1:"
PRE_CMD    = ""
If (LIST_ONLY) Then
  PRE_CMD  = "dir "
End If

' Globals
Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")

' Download a file from HTTP
Sub DownloadHttp(Url, File)
  Const BINARY = 1
  Const OVERWRITE = 2
  Set xHttp = createobject("Microsoft.XMLHTTP")
  Set bStrm = createobject("Adodb.Stream")
  Call xHttp.Open("GET", Url, False)
  If NO_CACHE = True Then
    Call xHttp.SetRequestHeader("If-None-Match", "some-random-string")
    Call xHttp.SetRequestHeader("Cache-Control", "no-cache,max-age=0")
    Call xHttp.SetRequestHeader("Pragma", "no-cache")
  End If
  Call xHttp.Send()
  If Not xHttp.Status = 200 Then
    Call WScript.Echo("Unable to access file - Error " & xHttp.Status)
    Call WScript.Quit(1)
  End If
  With bStrm
    .type = BINARY
    .open
    .write xHttp.responseBody
    .savetofile File, OVERWRITE
  End With
End Sub

' Unzip a specific file from an archive
Sub Unzip(Archive, File)
  Const NOCONFIRMATION = &H10&
  Const NOERRORUI = &H400&
  Const SIMPLEPROGRESS = &H100&
  unzipFlags = NOCONFIRMATION + NOERRORUI + SIMPLEPROGRESS
  Set objShell = CreateObject("Shell.Application")
  Set objSource = objShell.NameSpace(fso.GetAbsolutePathName(Archive)).Items()
  Set objTarget = objShell.NameSpace(fso.GetAbsolutePathName("."))
  ' Only extract the file we are interested in
  For i = 0 To objSource.Count - 1
    If objSource.Item(i).Name = File Then
      Call objTarget.CopyHere(objSource.Item(i), unzipFlags)
    End If
  Next
End Sub

' Check that QEMU is available
If Not fso.FileExists(QEMU_PATH & QEMU_EXE) Then
  Call WScript.Echo("'" & QEMU_PATH & QEMU_EXE & "' was not found." & vbCrLf &_
    "Please make sure QEMU is installed or edit the path in '.msvc\debug.vbs'.")
  Call WScript.Quit(1)
End If

' Fetch the UEFI firmware and unzip it
If Not fso.FileExists(FW_FILE) Then
  Call WScript.Echo("The UEFI firmware file, needed for QEMU, " &_
    "will be downloaded from: " & FW_URL & vbCrLf & vbCrLf &_
    "Note: Unless you delete the file, this should only happen once.")
  Call DownloadHttp(FW_URL, FW_ZIP)
End If
If Not fso.FileExists(FW_ZIP) And Not fso.FileExists(FW_FILE) Then
  Call WScript.Echo("There was a problem downloading the QEMU UEFI firmware.")
  Call WScript.Quit(1)
End If
If fso.FileExists(FW_ZIP) Then
  Call Unzip(FW_ZIP, FW_BASE & ".fd")
  Call fso.MoveFile(FW_BASE & ".fd", FW_FILE)
  Call fso.DeleteFile(FW_ZIP)
End If
If Not fso.FileExists(FW_FILE) Then
  Call WScript.Echo("There was a problem unzipping the QEMU UEFI firmware.")
  Call WScript.Quit(1)
End If

' Fetch the VHD image
If Not fso.FileExists(IMG) Then
  Call DownloadHttp(IMG_URL, IMG_ZIP)
  Call Unzip(IMG_ZIP, IMG)
  Call fso.DeleteFile(IMG_ZIP)
End If
If Not fso.FileExists(IMG) Then
  Call WScript.Echo("There was a problem downloading or unzipping the " & FS & " image.")
  Call WScript.Quit(1)
End If

' Copy the files where required, and start QEMU
' Note: Linaro's QEMU-EFI.fd firmware is very sensitive about '/' vs '\'
Call shell.Run("%COMSPEC% /c mkdir ""image\efi\boot""", 0, True)
If USE_EDK2 Then
  Call fso.CopyFile("Build\RELEASE_VS2019\" & EDK_ARCH & "\" & FS & ".efi", "image\" & DRV, True)
Else
  Call fso.CopyFile(BIN, "image\" & DRV, True)
End If
' Create a startup.nsh that: sets logging, loads the driver and executes an "Hello World" app from the disk
Set file = fso.CreateTextFile("image\efi\boot\startup.nsh", True)
Call file.Write("set FS_LOGGING " & LOG_LEVEL & vbCrLf &_
  "load fs0:\" & DRV & vbCrLf &_
  "map -r" & vbCrLf &_
  PRE_CMD & MNT & "\EFI\Boot\boot" & UEFI_EXT & ".efi" & vbCrLf)
Call file.Close()
Call shell.Run("""" & QEMU_PATH & QEMU_EXE & """ " & QEMU_OPTS & " -L . -bios " & FW_FILE & " -hda fat:rw:image -hdb " & IMG, 1, True)
