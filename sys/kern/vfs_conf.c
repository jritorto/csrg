/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	@(#)vfs_conf.c	7.7 (Berkeley) 05/14/92
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#ifdef FFS
#include <ufs/ffs/ffs_extern.h>

/*
 * This specifies the filesystem used to mount the root.
 * This specification should be done by /etc/config.
 */
int (*mountroot)() = ffs_mountroot;
#endif

/*
 * These define the root filesystem and device.
 */
struct mount *rootfs;
struct vnode *rootdir;

/*
 * Set up the filesystem operations for vnodes.
 * The types are defined in mount.h.
 */
#ifdef FFS
extern	struct vfsops ufs_vfsops;
#define	UFS_VFSOPS	&ufs_vfsops
#else
#define	UFS_VFSOPS	NULL
#endif

#ifdef LFS
extern	struct vfsops lfs_vfsops;
#define	LFS_VFSOPS	&lfs_vfsops
#else
#define	LFS_VFSOPS	NULL
#endif

#ifdef MFS
extern	struct vfsops mfs_vfsops;
#define	MFS_VFSOPS	&mfs_vfsops
#else
#define	MFS_VFSOPS	NULL
#endif

#ifdef NFS
extern	struct vfsops nfs_vfsops;
#define	NFS_VFSOPS	&nfs_vfsops
#else
#define	NFS_VFSOPS	NULL
#endif

struct vfsops *vfssw[] = {
	NULL,			/* 0 = MOUNT_NONE */
	UFS_VFSOPS,		/* 1 = MOUNT_UFS */
	NFS_VFSOPS,		/* 2 = MOUNT_NFS */
	MFS_VFSOPS,		/* 3 = MOUNT_MFS */
	NULL,			/* 4 = MOUNT_PC */
	LFS_VFSOPS,		/* 5 = MOUNT_LFS */
};
