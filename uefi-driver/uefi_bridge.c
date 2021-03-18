/* uefi_bridge.c - libntfs-3g interface for UEFI */
/*
 *  Copyright © 2021 Pete Batard <pete@akeo.ie>
 *
 *  Parts taken from lowntfs-3g.c:
 *  Copyright © 2005-2007 Yura Pakhuchiy
 *  Copyright © 2005 Yuval Fledel
 *  Copyright © 2006-2009 Szabolcs Szakacsits
 *  Copyright © 2007-2021 Jean-Pierre Andre
 *  Copyright © 2009 Erik Larsson
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

#include "compat.h"
#include "volume.h"
#include "unistr.h"
#include "logging.h"
#include "dir.h"

#include "uefi_driver.h"
#include "uefi_bridge.h"
#include "uefi_logging.h"
#include "uefi_support.h"

/* Not all platforms have this errno */
#ifndef ENOMEDIUM
#define ENOMEDIUM 159
#endif

#define IS_DIR(ni)      (((ntfs_inode*)(ni))->mrec->flags & MFT_RECORD_IS_DIRECTORY)
#define IS_DIRTY(ni)    (NInoDirty((ntfs_inode*)(ni)) || NInoAttrListDirty((ntfs_inode*)(ni)))

static inline int _to_utf8(CONST CHAR16* Src, char** dst, const char* function)
{
	/* ntfs_ucstombs() can be used to convert to UTF-8 */
	int sz = ntfs_ucstombs(Src, SafeStrLen(Src), dst, 0);
	if (sz < 0)
		PrintError(L"%a failed to convert '%s': %a\n",
			function, Src, strerror(errno));
	return sz;
}

#define to_utf8(Src, dst) _to_utf8(Src, dst, __FUNCTION__)

/*
 * Convert an errno to an EFI_STATUS code. Adapted from:
 * https://github.com/ipxe/ipxe/blob/master/src/include/ipxe/errno/efi.h
 */
static EFI_STATUS ErrnoToEfiStatus(VOID)
{
	switch (errno) {
	case 0:
		return EFI_SUCCESS;
	case ECANCELED:
		return EFI_ABORTED;
	case EACCES:
	case EEXIST:
	case ETXTBSY:
		return EFI_ACCESS_DENIED;
	case EADDRINUSE:
	case EALREADY:
	case EINPROGRESS:
	case EISCONN:
		return EFI_ALREADY_STARTED;
	case EMSGSIZE:
		return EFI_BAD_BUFFER_SIZE;
	case E2BIG:
	case EOVERFLOW:
	case ERANGE:
		return EFI_BUFFER_TOO_SMALL;
	case ENODEV:
		return EFI_DEVICE_ERROR;
	case ENOEXEC:
		return EFI_LOAD_ERROR;
	case ESPIPE:
		return EFI_END_OF_FILE;
	case EFBIG:
		return EFI_END_OF_MEDIA;
	case EBADF:
	case EDOM:
	case EFAULT:
	case EIDRM:
	case EILSEQ:
	case EINVAL:
	case ENAMETOOLONG:
	case EPROTOTYPE:
		return EFI_INVALID_PARAMETER;
	case EMFILE:
	case EMLINK:
	case ENFILE:
	case ENOBUFS:
	case ENOLCK:
	case ENOLINK:
	case ENOMEM:
	case ENOSR:
		return EFI_OUT_OF_RESOURCES;
	case EBADMSG:
	case EISDIR:
	case EIO:
	case ENOMSG:
	case ENOSTR:
	case EPROTO:
		return EFI_PROTOCOL_ERROR;
	case EBUSY:
	case ENODATA:
		return EFI_NO_RESPONSE;
	case ECHILD:
	case ENOENT:
	case ENXIO:
		return EFI_NOT_FOUND;
	case EAGAIN:
	case EINTR:
		return EFI_NOT_READY;
	case ESRCH:
		return EFI_NOT_STARTED;
	case ETIME:
	case ETIMEDOUT:
		return EFI_TIMEOUT;
	case EAFNOSUPPORT:
	case ENOPROTOOPT:
	case ENOSYS:
	case ENOTSUP:
		return EFI_UNSUPPORTED;
	case ENOMEDIUM:
		return EFI_NO_MEDIA;
	case ELOOP:
	case ENOTDIR:
	case ENOTEMPTY:
	case EXDEV:
		return EFI_VOLUME_CORRUPTED;
	case ENOSPC:
		return EFI_VOLUME_FULL;
	case EROFS:
		return EFI_WRITE_PROTECTED;
	case EPERM:
		return EFI_SECURITY_VIOLATION;
	default:
		return EFI_NO_MAPPING;
	}
}

/*
 * Set errno from an EFI_STATUS code
 */
VOID NtfsSetErrno(EFI_STATUS Status)
{
	switch (Status) {
	case EFI_SUCCESS:
		errno = 0; break;
	case EFI_LOAD_ERROR:
		errno = ENOEXEC; break;
	case EFI_INVALID_PARAMETER:
		errno = EINVAL; break;
	case EFI_UNSUPPORTED:
		errno = ENOTSUP; break;
	case EFI_BAD_BUFFER_SIZE:
		errno = EMSGSIZE; break;
	case EFI_BUFFER_TOO_SMALL:
		errno = E2BIG; break;
	case EFI_NOT_READY:
		errno = EAGAIN; break;
	case EFI_DEVICE_ERROR:
		errno = ENODEV; break;
	case EFI_MEDIA_CHANGED:
	case EFI_NO_MEDIA:
		errno = ENOMEDIUM; break;
	case EFI_WRITE_PROTECTED:
		errno = EROFS; break;
	case EFI_OUT_OF_RESOURCES:
		errno = ENOMEM; break;
	case EFI_VOLUME_CORRUPTED:
		errno = EXDEV; break;
	case EFI_VOLUME_FULL:
		errno = ENOSPC; break;
	case EFI_NOT_FOUND:
		errno = ENOENT; break;
	case EFI_ACCESS_DENIED:
		errno = EACCES; break;
	case EFI_NO_RESPONSE:
		errno = EBUSY; break;
	case EFI_TIMEOUT:
		errno = ETIMEDOUT; break;
	case EFI_NOT_STARTED:
		errno = ESRCH; break;
	case EFI_ALREADY_STARTED:
		errno = EALREADY; break;
	case EFI_ABORTED:
		errno = ECANCELED; break;
	case EFI_ICMP_ERROR:
	case EFI_TFTP_ERROR:
	case EFI_CRC_ERROR:
	case EFI_PROTOCOL_ERROR:
	case EFI_INVALID_LANGUAGE:
		errno = EPROTO; break;
	case EFI_INCOMPATIBLE_VERSION:
		errno = ENOEXEC; break;
	case EFI_SECURITY_VIOLATION:
		errno = EPERM; break;
	case EFI_END_OF_MEDIA:
		errno = EFBIG; break;
	case EFI_END_OF_FILE:
		errno = ESPIPE; break;
	case EFI_COMPROMISED_DATA:
	case EFI_NO_MAPPING:
	default:
		errno = EFAULT; break;
	}
}

/*
 * Compute an EFI_TIME representation of an ntfs_time field
 */
VOID
NtfsGetEfiTime(EFI_NTFS_FILE* File, EFI_TIME* Time, INTN Type)
{
	ntfs_inode* ni = (ntfs_inode*)File->NtfsInode;
	ntfs_time time = NTFS_TIME_OFFSET;

	FS_ASSERT(ni != NULL);

	if (ni != NULL) {
		switch (Type) {
		case TIME_CREATED:
			time = ni->creation_time;
			break;
		case TIME_ACCESSED:
			time = ni->last_access_time;
			break;
		case TIME_MODIFIED:
			time = ni->last_data_change_time;
			break;
		default:
			FS_ASSERT(TRUE);
			break;
		}
	}

	UnixTimeToEfiTime(NTFS_TO_UNIX_TIME(time), Time);
}

/*
 * Translate a UEFI driver log level into a libntfs-3g log level.
 */
VOID
NtfsSetLogger(UINTN Level)
{
	/* Critical log level is always enabled */
	UINT32 levels = NTFS_LOG_LEVEL_CRITICAL;

	if (Level >= FS_LOGLEVEL_ERROR)
		levels |= NTFS_LOG_LEVEL_ERROR | NTFS_LOG_LEVEL_PERROR;
	if (Level >= FS_LOGLEVEL_WARNING)
		levels |= NTFS_LOG_LEVEL_WARNING;
	if (Level >= FS_LOGLEVEL_INFO)
		levels |= NTFS_LOG_LEVEL_INFO | NTFS_LOG_LEVEL_VERBOSE | NTFS_LOG_LEVEL_PROGRESS;
	if (Level >= FS_LOGLEVEL_DEBUG)
		levels |= NTFS_LOG_LEVEL_DEBUG | NTFS_LOG_LEVEL_QUIET;
	if (Level >= FS_LOGLEVEL_EXTRA)
		levels |= NTFS_LOG_LEVEL_TRACE;

	ntfs_log_clear_flags(UINT32_MAX);
	/* If needed, NTFS_LOG_FLAG_FILENAME | NTFS_LOG_FLAG_LINE can be added */
	ntfs_log_set_flags(NTFS_LOG_FLAG_PREFIX);
	ntfs_log_clear_levels(UINT32_MAX);
	ntfs_log_set_levels(levels);
}

BOOLEAN
NtfsIsVolumeReadOnly(VOID* NtfsVolume)
{
#ifdef FORCE_READONLY
	/* NVolReadOnly() should apply, but just to be safe... */
	return TRUE;
#else
	ntfs_volume* vol = (ntfs_volume*)NtfsVolume;
	return NVolReadOnly(vol);
#endif
}

/*
 * Soooooooo.... we have to perform our own caching here, because ntfs-3g
 * is not designed to handle double open, and the UEFI Shell *DOES* some
 * weird stuff, such as opening the same file twice, first rw then ro,
 * while keeping the rw instance opened, as well as other very illogical
 * things. Which means that, if we just hook these into ntfs_open_inode()
 * calls, all kind of bad things related to caching are supposed to happen.
 * Ergo, we need to keep a list of all the files we already have an inode
 * for, and perform look up to prevent double inode open.
 */

/* A file lookup entry */
typedef struct {
	LIST_ENTRY* ForwardLink;
	LIST_ENTRY* BackLink;
	EFI_NTFS_FILE* File;
} LookupEntry;

/*
 * Look for an existing file instance in our list, either
 * by matching a File->Path (if Inum is 0) or the inode
 * number specified in Inum.
 * IgnoreSelf can be used if you want to prevent the file
 * passed as parameter from matching (in case you are using
 * it with an altered path for instance).
 * Returns a pointer to the file instance when found, NULL
 * if not found.
 */
static EFI_NTFS_FILE*
NtfsLookup(EFI_NTFS_FILE* File, UINT64 Inum, BOOLEAN IgnoreSelf)
{
	LookupEntry* ListHead = (LookupEntry*)&File->FileSystem->LookupListHead;
	LookupEntry* Entry;
	ntfs_inode* ni;

	for (Entry = (LookupEntry*)ListHead->ForwardLink;
		Entry != ListHead;
		Entry = (LookupEntry*)Entry->ForwardLink) {
		FS_ASSERT(Entry->File->NtfsInode != NULL);
		if (Inum == 0) {
			/* If IgnoreSelf is active, prevent param from matching */
			if (IgnoreSelf && Entry->File == File)
				continue;
			/* An empty path should return the root */
			if (File->Path[0] == 0 && Entry->File->IsRoot)
				return Entry->File;
			if (StrCmp(File->Path, Entry->File->Path) == 0)
				return Entry->File;
		} else {
			ni = Entry->File->NtfsInode;
			if (ni->mft_no == GetInodeNumber(Inum))
				return Entry->File;
		}
	}
	return NULL;
}

/* Shorthands for the above */
#define NtfsLookupPath(File, IgnoreSelf) NtfsLookup(File, 0, IgnoreSelf)
#define NtfsLookupInum(File, Inum) NtfsLookup(File, Inum, FALSE)

/*
 * Convenience call to look for an open parent file instance
 */
static __inline EFI_NTFS_FILE*
NtfsLookupParent(EFI_NTFS_FILE* File)
{
	EFI_NTFS_FILE* Parent;

	/* BaseName always points into a non empty Path */
	FS_ASSERT(File->BaseName[-1] == PATH_CHAR);
	File->BaseName[-1] = 0;
	Parent = NtfsLookupPath(File, TRUE);
	File->BaseName[-1] = PATH_CHAR;
	return Parent;
}

/*
 * Add a new file instance to the lookup list
 */
static VOID
NtfsLookupAdd(EFI_NTFS_FILE* File)
{
	LIST_ENTRY* ListHead = &File->FileSystem->LookupListHead;
	LookupEntry* Entry = AllocatePool(sizeof(LookupEntry));

	if (Entry) {
		Entry->File = File;
		InsertTailList(ListHead, (LIST_ENTRY*)Entry);
	}
}

/*
 * Remove an existing file instance from the lookup list
 */
static VOID
NtfsLookupRem(EFI_NTFS_FILE* File)
{
	LookupEntry* ListHead = (LookupEntry*)&File->FileSystem->LookupListHead;
	LookupEntry* Entry;

	for (Entry = (LookupEntry*)ListHead->ForwardLink;
		Entry != ListHead;
		Entry = (LookupEntry*)Entry->ForwardLink) {
		if (File == Entry->File) {
			RemoveEntryList((LIST_ENTRY*)Entry);
			FreePool(Entry);
			return;
		}
	}
}

/*
 * Clear the lookup list and free all allocated resources
 */
static VOID
NtfsLookupFree(LIST_ENTRY* List)
{
	LookupEntry *ListHead = (LookupEntry*)List, *Entry;

	for (Entry = (LookupEntry*)ListHead->ForwardLink;
		Entry != ListHead;
		Entry = (LookupEntry*)Entry->ForwardLink) {
		RemoveEntryList((LIST_ENTRY*)Entry);
		FreePool(Entry);
	}
}

/*
 * Wrapper for ntfs_pathname_to_inode()
 *
 * Unlike what FUSE does, we really can't use ntfs_pathname_to_inode()
 * with a NULL dir_ni in UEFI because we always run into a situation
 * where inodes between the inode we want and root are still open and
 * ntfs-3g is (officially) very averse to reopening any inode, ever,
 * which it would end up doing internally during directory traversal.
 *
 * So we must make sure that there aren't any inodes open between our
 * target and the directory we start the path search with, by going
 * down our path until we either end up with a directory instance that
 * we already have open, or root.
 *
 * It should be pointed out that there is no guarantee that an open
 * root instance exists while performing this search, as the UEFI
 * Shell is wont to close root before it closes other files.
 */
static ntfs_inode*
NtfsOpenInodeFromPath(EFI_FS* FileSystem, CONST CHAR16* Path)
{
	EFI_NTFS_FILE* Parent = NULL, File = { 0 };
	char* path = NULL;
	ntfs_inode* ni = NULL;
	INTN Len = SafeStrLen(Path);
	CHAR16* TmpPath = NULL;
	int sz;

	/* Special case for root */
	if (Path[0] == 0 || (Path[0] == PATH_CHAR && Path[1] == 0))
		return ntfs_inode_open(FileSystem->NtfsVolume, FILE_root);

	TmpPath = StrDup(Path);
	if (TmpPath == NULL)
		return NULL;

	FS_ASSERT(TmpPath[0] == PATH_CHAR);
	FS_ASSERT(TmpPath[1] != 0);

	/* Create a mininum file we can use for lookup */
	File.Path = TmpPath;
	File.FileSystem = FileSystem;

	/* Go down the path to find the closest open directory */
	while (Parent == NULL && Len > 0) {
		while (TmpPath[--Len] != PATH_CHAR);
		TmpPath[Len] = 0;
		Parent = NtfsLookupPath(&File, FALSE);
		TmpPath[Len] = PATH_CHAR;
	}

	/* Convert the remainer of the path to relative from Parent */
	sz = to_utf8(&TmpPath[Len + 1], &path);
	FreePool(TmpPath);
	if (sz < 0)
		return NULL;

	/* An empty path below is fine and will return the root inode */
	ni = ntfs_pathname_to_inode(FileSystem->NtfsVolume, Parent ? Parent->NtfsInode : NULL, path);
	free(path);
	return ni;
}

/*
 * Mount an NTFS volume an initilize the related attributes
 */
EFI_STATUS
NtfsMountVolume(EFI_FS* FileSystem)
{
	EFI_STATUS Status = EFI_SUCCESS;
	ntfs_volume* vol = NULL;
	ntfs_mount_flags flags = NTFS_MNT_EXCLUSIVE | NTFS_MNT_IGNORE_HIBERFILE | NTFS_MNT_MAY_RDONLY;
	char* device = NULL;

	/* Don't double mount a volume */
	if (FileSystem->MountCount++ > 0)
		return EFI_SUCCESS;

#ifdef FORCE_READONLY
	flags |= NTFS_MNT_RDONLY;
#endif

	if (to_utf8(FileSystem->DevicePathString, &device) < 0)
		return ErrnoToEfiStatus();

	/* Insert this filesystem in our list so that ntfs_mount() can locate it */
	InsertTailList(&FsListHead, (LIST_ENTRY*)FileSystem);

	/* Initialize the Lookup List for this volume */
	InitializeListHead(&FileSystem->LookupListHead);

	ntfs_log_set_handler(ntfs_log_handler_uefi);

	vol = ntfs_mount(device, flags);
	free(device);

	/* Detect error conditions */
	if (vol == NULL) {
		switch (ntfs_volume_error(errno)) {
		case NTFS_VOLUME_CORRUPT:
			Status = EFI_VOLUME_CORRUPTED; break;
		case NTFS_VOLUME_LOCKED:
		case NTFS_VOLUME_NO_PRIVILEGE:
			Status = EFI_ACCESS_DENIED; break;
		case NTFS_VOLUME_OUT_OF_MEMORY:
			Status = EFI_OUT_OF_RESOURCES; break;
		default:
			Status = EFI_NOT_FOUND; break;
		}
		/* If we had a serial before, then the media was removed */
		if (FileSystem->NtfsVolumeSerial != 0)
			Status = EFI_NO_MEDIA;
	} else if ((FileSystem->NtfsVolumeSerial != 0) &&
		(vol->vol_serial != FileSystem->NtfsVolumeSerial)) {
		Status = EFI_MEDIA_CHANGED;
	}
	if (EFI_ERROR(Status)) {
		RemoveEntryList((LIST_ENTRY*)FileSystem);
		return Status;
	}

	/* Store the serial to detect media change/removal */
	FileSystem->NtfsVolumeSerial = vol->vol_serial;

	/* Population of free space must be done manually */
	ntfs_volume_get_free_space(vol);
	FileSystem->NtfsVolume = vol;
	ntfs_mbstoucs(vol->vol_name, &FileSystem->NtfsVolumeLabel);
	PrintInfo(L"Mounted volume '%s'\n", FileSystem->NtfsVolumeLabel);

	return EFI_SUCCESS;
}

/*
 * Unmount an NTFS volume and free allocated resources
 */
EFI_STATUS
NtfsUnmountVolume(EFI_FS* FileSystem)
{
	ntfs_umount(FileSystem->NtfsVolume, FALSE);

	PrintInfo(L"Unmounted volume '%s'\n", FileSystem->NtfsVolumeLabel);
	NtfsLookupFree(&FileSystem->LookupListHead);
	free(FileSystem->NtfsVolumeLabel);
	FileSystem->NtfsVolumeLabel = NULL;
	FileSystem->MountCount = 0;
	FileSystem->TotalRefCount = 0;

	RemoveEntryList((LIST_ENTRY*)FileSystem);

	return EFI_SUCCESS;
}

/*
 * Returns the amount of free space on the volume
 */
UINT64
NtfsGetVolumeFreeSpace(VOID* NtfsVolume)
{
	ntfs_volume* vol = (ntfs_volume*)NtfsVolume;

	ntfs_volume_get_free_space(vol);

	return vol->free_clusters * vol->cluster_size;
}

/*
 * Allocate a new EFI_NTFS_FILE data structure
 */
EFI_STATUS
NtfsAllocateFile(EFI_NTFS_FILE** File, EFI_FS* FileSystem)
{
	EFI_NTFS_FILE* NewFile;

	NewFile = AllocateZeroPool(sizeof(*NewFile));
	if (NewFile == NULL)
		return EFI_OUT_OF_RESOURCES;

	/* Initialize the attributes */
	NewFile->FileSystem = FileSystem;
	NewFile->EfiFile.Revision = EFI_FILE_PROTOCOL_REVISION2;
	NewFile->EfiFile.Open = FileOpen;
	NewFile->EfiFile.Close = FileClose;
	NewFile->EfiFile.Delete = FileDelete;
	NewFile->EfiFile.Read = FileRead;
	NewFile->EfiFile.Write = FileWrite;
	NewFile->EfiFile.GetPosition = FileGetPosition;
	NewFile->EfiFile.SetPosition = FileSetPosition;
	NewFile->EfiFile.GetInfo = FileGetInfo;
	NewFile->EfiFile.SetInfo = FileSetInfo;
	NewFile->EfiFile.Flush = FileFlush;
	NewFile->EfiFile.OpenEx = FileOpenEx;
	NewFile->EfiFile.ReadEx = FileReadEx;
	NewFile->EfiFile.WriteEx = FileWriteEx;
	NewFile->EfiFile.FlushEx = FileFlushEx;

	*File = NewFile;
	return EFI_SUCCESS;
}

/*
 * Free an allocated EFI_NTFS_FILE data structure
 */
VOID
NtfsFreeFile(EFI_NTFS_FILE* File)
{
	if (File == NULL)
		return;
	/* Only destroy a file that has no refs */
	if (File->RefCount <= 0) {
		FreePool(File->Path);
		FreePool(File);
	}
}

/*
 * Open or reopen a file instance
 */
EFI_STATUS
NtfsOpenFile(EFI_NTFS_FILE** FilePointer)
{
	EFI_NTFS_FILE* File;

	/* See if we already have a file instance open. */
	File = NtfsLookupPath(*FilePointer, FALSE);

	if (File != NULL) {
		/* Existing file instance found => Use that one */
		NtfsFreeFile(*FilePointer);
		*FilePointer = File;
		return EFI_SUCCESS;
	}

	/* Existing file instance was not found */
	File = *FilePointer;
	File->IsRoot = (File->Path[0] == PATH_CHAR && File->Path[1] == 0);
	File->NtfsInode = NtfsOpenInodeFromPath(File->FileSystem, File->Path);
	if (File->NtfsInode == NULL)
		return ErrnoToEfiStatus();
	File->IsDir = IS_DIR(File->NtfsInode);

	/* Add the new entry */
	NtfsLookupAdd(File);

	return EFI_SUCCESS;
}

/*
 * Close an open file
 */
VOID
NtfsCloseFile(EFI_NTFS_FILE* File)
{
	EFI_NTFS_FILE* Parent = NULL;
	u64 parent_inum = 0;

	if (File == NULL || File->NtfsInode == NULL)
		return;
	/*
	 * If the inode is dirty, ntfs_inode_close() will issue an
	 * ntfs_inode_sync() which may try to open the parent inode.
	 * Therefore, since ntfs-3g is not keen on reopen, if we do
	 * have the parent inode open, we need to close it first.
	 * Of course, the big question becomes: "But what if that
	 * parent's parent is also open and dirty?", which we assert
	 * it isn't...
	 */
	if (IS_DIRTY(File->NtfsInode)) {
		Parent = NtfsLookupParent(File);
		if (Parent != NULL) {
			parent_inum = ((ntfs_inode*)Parent->NtfsInode)->mft_no;
			ntfs_inode_close(Parent->NtfsInode);
		}
	}
	ntfs_inode_close(File->NtfsInode);
	if (Parent != NULL) {
		Parent->NtfsInode = ntfs_inode_open(File->FileSystem->NtfsVolume, parent_inum);
		if (Parent->NtfsInode == NULL) {
			PrintError(L"%a: Failed to reopen Parent: %a\n", __FUNCTION__, strerror(errno));
			NtfsLookupRem(Parent);
		}
	}
	NtfsLookupRem(File);
}

/*
 * Read the content of an existing directory
 */
EFI_STATUS
NtfsReadDirectory(EFI_NTFS_FILE* File, NTFS_DIRHOOK Hook, VOID* HookData)
{
	if (File->DirPos == -1)
		return EFI_END_OF_FILE;

	if (ntfs_readdir(File->NtfsInode, &File->DirPos, HookData, Hook)) {
		PrintError(L"%a failed: %a\n", __FUNCTION__, strerror(errno));
		return ErrnoToEfiStatus();
	}

	return EFI_SUCCESS;
}

/*
 * Read from an open file into a data buffer
 */
EFI_STATUS
NtfsReadFile(EFI_NTFS_FILE* File, VOID* Data, UINTN* Len)
{
	ntfs_attr* na = NULL;
	s64 max_read, size = *Len;

	*Len = 0;

	na = ntfs_attr_open(File->NtfsInode, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		PrintError(L"%a failed: %a\n", __FUNCTION__, strerror(errno));
		return ErrnoToEfiStatus();
	}

	max_read = na->data_size;
	if (File->Offset + size > max_read) {
		if (File->Offset > max_read) {
			/* Per UEFI specs */
			ntfs_attr_close(na);
			return EFI_DEVICE_ERROR;
		}
		size = max_read - File->Offset;
	}

	while (size > 0) {
		s64 ret = ntfs_attr_pread(na, File->Offset, size, &((UINT8*)Data)[*Len]);
		if (ret != size)
			PrintError(L"%a: Error reading inode %lld at offset %lld: %lld <> %lld",
				((ntfs_inode*)File->NtfsInode)->mft_no,
				File->Offset, *Len, ret);
		if (ret <= 0 || ret > size) {
			ntfs_attr_close(na);
			if (ret >= 0)
				errno = EIO;
			PrintError(L"%a failed: %a\n", __FUNCTION__, strerror(errno));
			return ErrnoToEfiStatus();
		}
		size -= ret;
		File->Offset += ret;
		*Len += ret;
	}

	ntfs_attr_close(na);

	if (!NtfsIsVolumeReadOnly(File->FileSystem->NtfsVolume))
		ntfs_inode_update_times(File->NtfsInode, NTFS_UPDATE_MCTIME);

	return EFI_SUCCESS;
}

/*
 * Return the current size occupied by a file
 */
UINT64
NtfsGetFileSize(EFI_NTFS_FILE* File)
{
	if (File->NtfsInode == NULL)
		return 0;
	return ((ntfs_inode*)File->NtfsInode)->data_size;
}

/*
 * Fill an EFI_FILE_INFO struct with data from the NTFS inode.
 * This function takes either a File or an MREF (with the MREF
 * being used if it's non-zero).
 */
EFI_STATUS
NtfsGetFileInfo(EFI_NTFS_FILE* File, EFI_FILE_INFO* Info, CONST UINT64 MRef, BOOLEAN IsDir)
{
	BOOLEAN NeedClose = FALSE;
	EFI_NTFS_FILE* Existing = NULL;
	ntfs_inode* ni = File->NtfsInode;

	/*
	 * If Non-zero MREF, we are listing a dir, in which case we need
	 * to open (and later close) the inode.
	 */
	if (MRef != 0) {
		Existing = NtfsLookupInum(File, MRef);
		if (Existing != NULL) {
			ni = Existing->NtfsInode;
		} else {
			ni = ntfs_inode_open(File->FileSystem->NtfsVolume, MRef);
			NeedClose = TRUE;
		}
	} else
		PrintExtra(L"NtfsGetInfo for inode: %lld\n", ni->mft_no);

	if (ni == NULL)
		return EFI_NOT_FOUND;

	Info->FileSize = ni->data_size;
	Info->PhysicalSize = ni->allocated_size;
	UnixTimeToEfiTime(NTFS_TO_UNIX_TIME(ni->creation_time), &Info->CreateTime);
	UnixTimeToEfiTime(NTFS_TO_UNIX_TIME(ni->last_access_time), &Info->LastAccessTime);
	UnixTimeToEfiTime(NTFS_TO_UNIX_TIME(ni->last_data_change_time), &Info->ModificationTime);

	Info->Attribute = 0;
	if (IsDir)
		Info->Attribute |= EFI_FILE_DIRECTORY;
	if (ni->flags & FILE_ATTR_READONLY || NtfsIsVolumeReadOnly(File->FileSystem->NtfsVolume))
		Info->Attribute |= EFI_FILE_READ_ONLY;
	if (ni->flags & FILE_ATTR_HIDDEN)
		Info->Attribute |= EFI_FILE_HIDDEN;
	if (ni->flags & FILE_ATTR_SYSTEM)
		Info->Attribute |= EFI_FILE_SYSTEM;
	if (ni->flags & FILE_ATTR_ARCHIVE)
		Info->Attribute |= EFI_FILE_ARCHIVE;

	if (NeedClose)
		ntfs_inode_close(ni);

	return EFI_SUCCESS;
}

/*
 * For extra safety, as well as in an effort to reduce the size of the
 * read-only driver executable, guard all the function calls that alter
 * volume data.
 */

#ifdef FORCE_READONLY

EFI_STATUS
NtfsCreateFile(EFI_NTFS_FILE** FilePointer)
{
	return EFI_WRITE_PROTECTED;
}

EFI_STATUS
NtfsDeleteFile(EFI_NTFS_FILE* File)
{
	return EFI_WRITE_PROTECTED;
}

EFI_STATUS
NtfsWriteFile(EFI_NTFS_FILE* File, VOID* Data, UINTN* Len)
{
	return EFI_WRITE_PROTECTED;
}

EFI_STATUS
NtfsSetFileInfo(EFI_NTFS_FILE* File, EFI_FILE_INFO* Info)
{
	return EFI_WRITE_PROTECTED;
}

EFI_STATUS
NtfsFlushFile(EFI_NTFS_FILE* File)
{
	return EFI_SUCCESS;
}

EFI_STATUS
NtfsRenameVolume(VOID* NtfsVolume, CONST CHAR16* Label, CONST INTN Len)
{
	return EFI_WRITE_PROTECTED;
}

#else /* FORCE_READONLY */

/*
 * Create new file or reopen an existing one
 */
EFI_STATUS
NtfsCreateFile(EFI_NTFS_FILE** FilePointer)
{
	EFI_STATUS Status;
	EFI_NTFS_FILE *File, *Parent = NULL;
	char* basename = NULL;
	ntfs_inode *dir_ni = NULL, *ni = NULL;
	int sz;

	/* If an existing open file instance is found, use that one */
	File = NtfsLookupPath(*FilePointer, FALSE);
	if (File != NULL) {
		/* Entries must be of the same type */
		if (File->IsDir != (*FilePointer)->IsDir)
			return EFI_ACCESS_DENIED;
		NtfsFreeFile(*FilePointer);
		*FilePointer = File;
		return EFI_SUCCESS;
	}

	/* No open instance for this inode => Open the parent inode */
	File = *FilePointer;

	/* Validate BaseName */
	if (ntfs_forbidden_names(File->FileSystem->NtfsVolume,
		File->BaseName, SafeStrLen(File->BaseName), TRUE)) {
		return EFI_INVALID_PARAMETER;
	}

	Parent = NtfsLookupParent(File);

	/* If the lookup failed, then the parent dir is not already open */
	if (Parent == NULL) {
		/* Isolate dirname and get the inode */
		FS_ASSERT(File->BaseName[-1] == PATH_CHAR);
		File->BaseName[-1] = 0;
		dir_ni = NtfsOpenInodeFromPath(File->FileSystem, File->Path);
		File->BaseName[-1] = PATH_CHAR;
	} else
		dir_ni = Parent->NtfsInode;

	if (dir_ni == NULL) {
		Status = ErrnoToEfiStatus();
		goto out;
	}

	/* Similar to FUSE: Deny creating into $Extend */
	if (dir_ni->mft_no == FILE_Extend) {
		Status = EFI_ACCESS_DENIED;
		goto out;
	}

	/* Find if the inode we are trying to create already exists */
	sz = to_utf8(File->BaseName, &basename);
	if (sz <= 0) {
		Status = ErrnoToEfiStatus();
		goto out;
	}
	/* We can safely call ntfs_pathname_to_inode since the inode is not open */
	ni = ntfs_pathname_to_inode(File->FileSystem->NtfsVolume, dir_ni, basename);
	if (ni != NULL) {
		/* Entries must be of the same type */
		if ((File->IsDir && !IS_DIR(ni)) || (!File->IsDir && IS_DIR(ni))) {
			Status = EFI_ACCESS_DENIED;
			goto out;
		}
	} else {
		/* Create the new file or directory */
		ni = ntfs_create(dir_ni, 0, File->BaseName,
			SafeStrLen(File->BaseName), File->IsDir ? S_IFDIR : S_IFREG);
		if (ni == NULL) {
			Status = ErrnoToEfiStatus();
			goto out;
		}
		/* Windows and FUSE set this flag by default */
		if (!File->IsDir)
			ni->flags |= FILE_ATTR_ARCHIVE;
	}

	/* Update cache lookup record */
	ntfs_inode_update_mbsname(dir_ni, basename, ni->mft_no);
	ntfs_inode_update_times(ni, NTFS_UPDATE_MCTIME);

	File->NtfsInode = ni;
	NtfsLookupAdd(File);
	Status = EFI_SUCCESS;

out:
	free(basename);
	/* NB: ntfs_inode_close(NULL) is fine */
	if (Parent == NULL)
		ntfs_inode_close(dir_ni);
	if EFI_ERROR(Status) {
		ntfs_inode_close(ni);
		File->NtfsInode = NULL;
	}
	return Status;
}

/*
 * Delete a file or directory from the volume
 *
 * Like FileDelete(), this call should only
 * return EFI_WARN_DELETE_FAILURE on error.
 */
EFI_STATUS
NtfsDeleteFile(EFI_NTFS_FILE* File)
{
	EFI_NTFS_FILE *Parent = NULL, *GrandParent = NULL;
	ntfs_inode* dir_ni;
	u64 parent_inum, grandparent_inum;
	int r;

	Parent = NtfsLookupParent(File);

	/* If the lookup failed, then the parent dir is not already open */
	if (Parent == NULL) {
		/* Isolate dirname and get the inode */
		FS_ASSERT(File->BaseName[-1] == PATH_CHAR);
		File->BaseName[-1] = 0;
		dir_ni = NtfsOpenInodeFromPath(File->FileSystem, File->Path);
		File->BaseName[-1] = PATH_CHAR;
		/* TODO: We may need to open the grandparent here too... */
		if (dir_ni == NULL)
			return ErrnoToEfiStatus();
	} else {
		/*
		 * ntfs-3g may attempt to reopen the file's grandparent, since it
		 * issue ntfs_inode_close on dir_ni which, when dir_ni is dirty,
		 * ultimately results in ntfs_inode_sync_file_name(dir_ni, NULL)
		 * which calls ntfs_inode_open(le64_to_cpu(fn->parent_directory))
		 * So we must make sure the grandparent's inode is closed...
		 */
		GrandParent = NtfsLookupParent(Parent);
		if (GrandParent != NULL) {
			if (GrandParent->IsRoot) {
				GrandParent = NULL;
			} else {
				grandparent_inum = ((ntfs_inode*)GrandParent->NtfsInode)->mft_no;
				ntfs_inode_close(GrandParent->NtfsInode);
			}
		}

		/* Parent dir was already open */
		dir_ni = Parent->NtfsInode;
		parent_inum = dir_ni->mft_no;
	}

	/* Similar to FUSE: Deny deleting from $Extend */
	if (dir_ni->mft_no == FILE_Extend)
		return EFI_ACCESS_DENIED;

	/* Delete the file */
	r = ntfs_delete(File->FileSystem->NtfsVolume, NULL, File->NtfsInode,
		dir_ni, File->BaseName, SafeStrLen(File->BaseName));
	NtfsLookupRem(File);
	if (r < 0) {
		PrintError(L"%a failed: %a\n", __FUNCTION__, strerror(errno));
		return EFI_WARN_DELETE_FAILURE;
	}
	File->NtfsInode = NULL;

	/* Reopen Parent or GrandParent if they were closed */
	if (Parent != NULL) {
		Parent->NtfsInode = ntfs_inode_open(File->FileSystem->NtfsVolume, parent_inum);
		if (Parent->NtfsInode == NULL) {
			PrintError(L"%a: Failed to reopen Parent: %a\n", __FUNCTION__, strerror(errno));
			NtfsLookupRem(Parent);
			return ErrnoToEfiStatus();
		}
	}
	if (GrandParent != NULL) {
		GrandParent->NtfsInode = ntfs_inode_open(File->FileSystem->NtfsVolume, grandparent_inum);
		if (GrandParent->NtfsInode == NULL) {
			PrintError(L"%a: Failed to reopen GrandParent: %a\n", __FUNCTION__, strerror(errno));
			NtfsLookupRem(GrandParent);
			return ErrnoToEfiStatus();
		}
	}

	return EFI_SUCCESS;
}

/*
 * Write from a data buffer into an open file
 */
EFI_STATUS
NtfsWriteFile(EFI_NTFS_FILE* File, VOID* Data, UINTN* Len)
{
	ntfs_inode* ni = File->NtfsInode;
	ntfs_attr* na = NULL;
	s64 size = *Len;

	*Len = 0;

	if (ni->flags & FILE_ATTR_READONLY)
		return EFI_WRITE_PROTECTED;

	na = ntfs_attr_open(File->NtfsInode, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		PrintError(L"%a failed (open): %a\n", __FUNCTION__, strerror(errno));
		return ErrnoToEfiStatus();
	}

	while (size > 0) {
		s64 ret = ntfs_attr_pwrite(na, File->Offset, size, &((UINT8*)Data)[*Len]);
		if (ret <= 0) {
			ntfs_attr_close(na);
			if (ret >= 0)
				errno = EIO;
			PrintError(L"%a failed (write): %a\n", __FUNCTION__, strerror(errno));
			return ErrnoToEfiStatus();
		}
		size -= ret;
		File->Offset += ret;
		*Len += ret;
	}

	ntfs_attr_close(na);

	ntfs_inode_update_times(File->NtfsInode, NTFS_UPDATE_MCTIME);

	return EFI_SUCCESS;
}

/*
 * Update NTFS inode data with the attributes from an EFI_FILE_INFO struct.
 */
EFI_STATUS
NtfsSetFileInfo(EFI_NTFS_FILE* File, EFI_FILE_INFO* Info)
{
	CONST EFI_TIME ZeroTime = { 0 };
	CHAR16* Path, * c;
	ntfs_inode* ni = File->NtfsInode;
	ntfs_attr* na;
	int r;

	PrintExtra(L"NtfsSetInfo for inode: %lld\n", ni->mft_no);

	/* Per UEFI specs, trying to change type should return access denied */
	if ((!IS_DIR(ni) && (Info->Attribute & EFI_FILE_DIRECTORY)) ||
		(IS_DIR(ni) && !(Info->Attribute & EFI_FILE_DIRECTORY)))
		return EFI_ACCESS_DENIED;

	/* If we get an absolute path, we might be moving the file */
	if (IS_PATH_DELIMITER(Info->FileName[0])) {
		/* Need to convert the path separators */
		Path = StrDup(Info->FileName);
		if (Path == NULL)
			return EFI_OUT_OF_RESOURCES;
		for (c = Path; *c; c++) {
			if (*c == DOS_PATH_CHAR)
				*c = PATH_CHAR;
		}
		CleanPath(Path);
		if (StrCmp(Path, File->Path)) {
			/* TODO: Add move support */
			return EFI_UNSUPPORTED;
		}
	}

	/* NtfsMoveFile() may have altered File->NtfsInode */
	ni = File->NtfsInode;

	if (!IS_DIR(ni) && (Info->FileSize != ni->data_size)) {
		na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
		if (!na) {
			PrintError(L"%a ntfs_attr_open failed: %a\n", __FUNCTION__, strerror(errno));
			return ErrnoToEfiStatus();
		}
		r = ntfs_attr_truncate(na, Info->FileSize);
		ntfs_attr_close(na);
		if (r) {
			PrintError(L"%a ntfs_attr_truncate failed: %a\n", __FUNCTION__, strerror(errno));
			return ErrnoToEfiStatus();
		}
	}

	/*
	 * Per UEFI specs: "A value of zero in CreateTime, LastAccess,
	 * or ModificationTime causes the fields to be ignored".
	 */
	if (CompareMem(&Info->CreateTime, &ZeroTime, sizeof(EFI_TIME)) != 0)
		ni->creation_time = UNIX_TO_NTFS_TIME(EfiTimeToUnixTime(&Info->CreateTime));
	if (CompareMem(&Info->LastAccessTime, &ZeroTime, sizeof(EFI_TIME)) != 0)
		ni->last_access_time = UNIX_TO_NTFS_TIME(EfiTimeToUnixTime(&Info->LastAccessTime));
	if (CompareMem(&Info->ModificationTime, &ZeroTime, sizeof(EFI_TIME)) != 0)
		ni->last_data_change_time = UNIX_TO_NTFS_TIME(EfiTimeToUnixTime(&Info->ModificationTime));

	ni->flags &= ~(FILE_ATTR_READONLY | FILE_ATTR_HIDDEN | FILE_ATTR_SYSTEM | FILE_ATTR_ARCHIVE);
	if (Info->Attribute & EFI_FILE_READ_ONLY)
		ni->flags |= FILE_ATTR_READONLY;
	if (Info->Attribute & EFI_FILE_HIDDEN)
		ni->flags |= FILE_ATTR_HIDDEN;
	if (Info->Attribute & EFI_FILE_SYSTEM)
		ni->flags |= FILE_ATTR_SYSTEM;
	if (Info->Attribute & EFI_FILE_ARCHIVE)
		ni->flags |= FILE_ATTR_ARCHIVE;

	/* No sync, since, per UEFI specs, change of attributes apply on close */
	return EFI_SUCCESS;
}

/*
 * Flush the current file
 */
EFI_STATUS
NtfsFlushFile(EFI_NTFS_FILE* File)
{
	EFI_STATUS Status = EFI_SUCCESS;
	EFI_NTFS_FILE* Parent = NULL;
	ntfs_inode* ni;
	u64 parent_inum;

	ni = File->NtfsInode;
	/* Nothing to do if the file is not dirty */
	if (!NInoDirty(ni) && NInoAttrListDirty(ni))
		return EFI_SUCCESS;

	/*
	 * Same story as with NtfsCloseFile, with the parent
	 * inode needing to be closed to be able to issue sync()
	 */
	Parent = NtfsLookupParent(File);
	if (Parent != NULL) {
		parent_inum = ((ntfs_inode*)Parent->NtfsInode)->mft_no;
		ntfs_inode_close(Parent->NtfsInode);
	}
	if (ntfs_inode_sync(File->NtfsInode) < 0) {
		PrintError(L"%a failed: %a\n", __FUNCTION__, strerror(errno));
		Status = ErrnoToEfiStatus();
	}
	if (Parent != NULL) {
		Parent->NtfsInode = ntfs_inode_open(File->FileSystem->NtfsVolume, parent_inum);
		if (Parent->NtfsInode == NULL) {
			PrintError(L"%a: Failed to reopen Parent: %a\n", __FUNCTION__, strerror(errno));
			NtfsLookupRem(Parent);
		}
	}
	return Status;
}

/*
 * Change the volume label.
 * Len is the length of the label, including terminating NUL character.
 */
EFI_STATUS
NtfsRenameVolume(VOID* NtfsVolume, CONST CHAR16* Label, CONST INTN Len)
{
	if (NtfsIsVolumeReadOnly(NtfsVolume))
		return EFI_WRITE_PROTECTED;
	if (ntfs_volume_rename(NtfsVolume, Label, (int)Len) < 0) {
		PrintError(L"%a failed: %a\n", __FUNCTION__, strerror(errno));
		return ErrnoToEfiStatus();
	}
	return EFI_SUCCESS;
}

#endif /* FORCE_READONLY */
