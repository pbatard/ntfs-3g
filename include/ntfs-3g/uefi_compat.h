/*
 * uefi_compat.h - Compatibility settings for the NTFS UEFI driver
 *
 * Copyright (c) 2021-2024 Pete Batard
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_COMPAT_UEFI_H
#define _NTFS_COMPAT_UEFI_H

/*
 * Expect these headers to always be present for
 * environments that can produce the UEFI driver.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#if defined(__MAKEWITH_GNUEFI)
#include <efi.h>
#define VA_LIST             va_list
#define VA_START            va_start
#define VA_END              va_end
#else
#include <Base.h>
#endif

#define HAVE_CLOCK_GETTIME  0
#define HAVE_ERRNO_H        1
#define HAVE_INTTYPES_H     1
#define HAVE_LIMITS_H       0
#define HAVE_STDARG_H       1
#define HAVE_STDBOOL_H      0
#define HAVE_STDDEF_H       0
#define HAVE_STDINT_H       1
#define HAVE_STDIO_H        1
#define HAVE_STDLIB_H       0
#define HAVE_SYS_STAT_H     1
#define HAVE_SYS_TYPES_H    1
#define HAVE_TIME_H         1
#define HAVE_WCHAR_H        0

/* Disable reparse plugins */
#define DISABLE_PLUGINS     1

/* Force UTF-8 always */
#define FORCE_UTF8          1

/* Don't update /etc/mtab */
#define IGNORE_MTAB         1

/* Enable GNU extensions on systems that have them. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE         1
#endif

/* All UEFI environments are little-endian */
#define WORDS_LITTLEENDIAN  1

/* System extended attributes mappings */
/* #undef XATTR_MAPPINGS */

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS   64

/* Maximum value for a signed 32-bit integer */
#define INT_MAX             2147483647

/* Inline keyword definitions */
#ifndef __cplusplus
#ifndef inline
#define inline              __inline
#endif
#ifndef __inline__
#define __inline__          __inline
#endif
#endif

#ifdef _MSC_VER
typedef unsigned int        dev_t;
typedef unsigned long       uid_t;
typedef unsigned long       gid_t;
typedef unsigned long       pid_t;
typedef unsigned short      mode_t;
typedef uint32_t            clockid_t;
#endif /* _MSC_VER */

struct group {
	char* gr_name;
	char* gr_passwd;
	gid_t gr_gid;
	char** gr_mem;
};

struct passwd {
	char* pw_name;
	char* pw_passwd;
	uid_t pw_uid;
	gid_t pw_gid;
	time_t pw_change;
	char* pw_class;
	char* pw_gecos;
	char* pw_dir;
	char* pw_shell;
	time_t pw_expire;
};

#ifndef O_RDONLY
#define O_RDONLY        0
#endif
#ifndef O_WRONLY
#define O_WRONLY        1
#endif
#ifndef O_RDWR
#define O_RDWR          2
#endif

#ifndef S_IXOTH
#define S_IXOTH         00000001
#endif
#ifndef S_IWOTH
#define S_IWOTH         00000002
#endif
#ifndef S_IROTH
#define S_IROTH         00000004
#endif
#ifndef S_IXGRP
#define S_IXGRP         00000010
#endif
#ifndef S_IWGRP
#define S_IWGRP         00000020
#endif
#ifndef S_IRGRP
#define S_IRGRP         00000040
#endif
#ifndef S_IXUSR
#define S_IXUSR         00000100
#endif
#ifndef S_IWUSR
#define S_IWUSR         00000200
#endif
#ifndef S_IRUSR
#define S_IRUSR         00000400
#endif
#ifndef S_ISVTX
#define S_ISVTX         00001000
#endif
#ifndef S_ISGID
#define S_ISGID         00002000
#endif
#ifndef S_ISUID
#define S_ISUID         00004000
#endif
#ifndef S_IFBLK
#define S_IFBLK         00060000
#endif
#ifndef S_IFIFO
#define S_IFIFO         00010000
#endif
#ifndef S_IFLNK
#define S_IFLNK         00120000
#endif
#ifndef S_IFSOCK
#define S_IFSOCK        00140000
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISREG
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME  0
#endif

/*
 * For a UEFI driver, we can hardcode these.
 */
#define getuid()        0
#define getgid()        0
#define getpid()        1
#define getgrgid(x)     NULL
#define getpwuid(x)     NULL
#define major(x)        0
#define minor(x)        0
/* atoi() is only ever used to convert uid/gid */
#define atoi(x)         0

/*
 * The following calls are only referenced when creating a
 * file system, which we'll never do with the UEFI driver.
 */
#define random()        0
#define srandom(x)

/*
 * Likewise makedev is pointless for us, so we define a simplified version.
 */
#define makedev(major, minor) (((major & 0xffff) << 16) | (minor & 0xffff))

/*
 * Freeze execution with a message on exit condition
 */
#define exit(x)         do {ntfs_log_critical("Driver exit requested!\n"); while(1);} while(0)

/*
 * open/close/read are referenced, but aren't expected to be called.
 */
static __inline__ int open(const char* pathname, int flags)
{
	errno = ENOSYS;
	return -1;
}

static __inline__ int close(int fd)
{
	errno = ENOSYS;
	return -1;
}

static __inline__ int read(int fildes, void* buf, size_t nbyte)
{
	errno = ENOSYS;
	return -1;
}

/*
 * Declaration of the standard UEFI AsciiVSPrint() and AsciiPrint() calls per:
 * https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Library/PrintLib.h
 */
UINTN EFIAPI AsciiVSPrint(CHAR8 *StartOfBuffer, UINTN BufferSize,
                                 CONST CHAR8 *FormatString, VA_LIST Marker);
UINTN EFIAPI AsciiPrint(const CHAR8 *Format, ...);

int clock_gettime(clockid_t clk_id, struct timespec* now);

/*
 * Eventually, we should prefix all those with ntfs_ and switch the
 * library to only using the ntfs_ prefixed calls, so that we can
 * redefine them without conflicts.
 */
void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* p, size_t new_size);
int   memcmp(const void* s1, const void* s2, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void  free(void* a);

size_t strlen(const char* s);
int   strcmp(const char* s1, const char* s2);
int   strncmp(const char* s1, const char* s2, size_t n);
char* strcat(char* dest, const char* src);
char* strdup(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
int   snprintf(char* str, size_t size, const char* format, ...);
char* strerror(int errnum);
/* errno -> _errno() / __errno_location() is defined in errno.h */
#ifdef _MSC_VER
int* _errno(void);
#else
int* __errno_location(void);
#endif

#endif /* defined _NTFS_COMPAT_UEFI_H */
