/*-
 * Copyright (c) 1983, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fcntl.h	5.10 (Berkeley) 02/05/91
 */

#ifndef _FCNTL_H_
#define	_FCNTL_H_

#ifndef KERNEL
#include <sys/types.h>
#endif

/* command values for fcntl(2) */
#define	F_DUPFD		0		/* duplicate file descriptor */
#define	F_GETFD		1		/* get file descriptor flags */
#define	F_SETFD		2		/* set file descriptor flags */
#define	F_GETFL		3		/* get file status flags */
#define	F_SETFL		4		/* set file status flags */
#ifndef _POSIX_SOURCE
#define	F_GETOWN	5		/* get SIGIO/SIGURG proc/pgrp */
#define F_SETOWN	6		/* set SIGIO/SIGURG proc/pgrp */
#endif
#define	F_GETLK		7		/* get record locking information */
#define	F_SETLK		8		/* set record locking information */
#define	F_SETLKW	9		/* F_SETLK; wait if blocked */

/* file descriptor flags (F_GETFD, F_SETFD) */
#define	FD_CLOEXEC	1		/* close-on-exec flag */

/* record locking flags (F_GETLK, F_SETLK, F_SETLKW) */
#define	F_RDLCK		1		/* shared or read lock */
#define	F_UNLCK		2		/* unlock */
#define	F_WRLCK		3		/* exclusive or write lock */
#ifdef KERNEL
#define	F_WAIT		0x010		/* Wait until lock is granted */
#define	F_FLOCK		0x020	 	/* Use flock(2) semantics for lock */
#define	F_POSIX		0x040	 	/* Use POSIX semantics for lock */
#endif

#ifndef _POSIX_SOURCE
/* lock operations for flock(2) */
#define	LOCK_SH		0x01		/* shared file lock */
#define	LOCK_EX		0x02		/* exclusive file lock */
#define	LOCK_NB		0x04		/* don't block when locking */
#define	LOCK_UN		0x08		/* unlock file */
#endif

/* file status flags */
#define	O_RDONLY	00000		/* open for reading only */
#define	O_WRONLY	00001		/* open for writing only */
#define	O_RDWR		00002		/* open for reading and writing */
#define	O_NONBLOCK	00004		/* no delay */
#ifndef _POSIX_SOURCE
#define	O_NDELAY	O_NONBLOCK
#define	FNDELAY		O_NONBLOCK
#endif
/*			00008		/* unused */
#define	O_APPEND	00010		/* set append mode */
#ifndef _POSIX_SOURCE
#define	FAPPEND		O_APPEND
#endif
					/* kernel placeholders */
#if !defined(_POSIX_SOURCE) && defined(KERNEL)
#define	O_MARK		00020		/* mark during gc() */
#define	O_DEFER		00040		/* defer for next gc pass */
#endif
/*			00080		/* unused */
#ifndef _POSIX_SOURCE
#define	O_ASYNC		00100		/* signal pgrp when data ready */
#define	FASYNC		O_ASYNC
#define	O_SHLOCK	00200		/* shared file lock present */
#define	O_EXLOCK	00400		/* exclusive file lock present */
/*			00800		/* unused */
#endif
#define	O_CREAT		01000		/* create if nonexistant */
#define	O_TRUNC		02000		/* truncate to zero length */
#define	O_EXCL		04000		/* error if already exists */
/*			08000		/* unused */

/* defined by POSIX 1003.1; BSD default, so no bit required */
#define	O_NOCTTY	0		/* don't assign controlling terminal */

/* mask for file access modes */
#define	O_ACCMODE	(O_RDONLY|O_WRONLY|O_RDWR)

/* file segment locking set data type - information passed to system by user */
struct flock {
	short	l_type;
	short	l_whence;
	size_t	l_start;
	size_t	l_len;		/* len = 0 means until end of file */
	short	l_sysid;
	int	l_pid;		/* should be pid_t */
};

#ifndef KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int	creat __P((const char *, mode_t));
int	fcntl __P((int, int, ...));
int	open __P((const char *, int, ...));
__END_DECLS
#endif

#endif /* !_FCNTL_H_ */
