/*
 * Copyright (c) 1986, 1989, 1991 Regents of the University of California.
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
 *	@(#)lfs_inode.c	7.70 (Berkeley) 07/05/92
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <vm/vm.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

int
lfs_init()
{
#ifdef VERBOSE
	printf("lfs_init\n");
#endif
	return (ufs_init());
}

/* Search a block for a specific dinode. */
struct dinode *
lfs_ifind(fs, ino, dip)
	struct lfs *fs;
	ino_t ino;
	register struct dinode *dip;
{
	register int cnt;
	register struct dinode *ldip;

#ifdef VERBOSE
	printf("lfs_ifind: inode %d\n", ino);
#endif
	for (cnt = INOPB(fs), ldip = dip + (cnt - 1); cnt--; --ldip)
		if (ldip->di_inum == ino)
			return (ldip);

	panic("lfs_ifind: dinode %u not found", ino);
	/* NOTREACHED */
}

int
lfs_update(ap)
	struct vop_update_args /* {
		struct vnode *a_vp;
		struct timeval *a_ta;
		struct timeval *a_tm;
		int a_waitfor;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip;

#ifdef VERBOSE
	printf("lfs_update\n");
#endif
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(vp);
	if ((ip->i_flag & (IUPD|IACC|ICHG|IMOD)) == 0)
		return (0);
	if (ip->i_flag&IACC)
		ip->i_atime.ts_sec = ap->a_ta->tv_sec;
	if (ip->i_flag&IUPD) {
		ip->i_mtime.ts_sec = ap->a_tm->tv_sec;
		(ip)->i_modrev++;
	}
	if (ip->i_flag&ICHG)
		ip->i_ctime.ts_sec = time.tv_sec;
	ip->i_flag &= ~(IUPD|IACC|ICHG|IMOD);

	/* Push back the vnode and any dirty blocks it may have. */
	return (ap->a_waitfor ? lfs_vflush(vp) : 0);
}

/* Update segment usage information when removing a block. */
#define UPDATE_SEGUSE \
	if (lastseg != -1) { \
		LFS_SEGENTRY(sup, fs, lastseg, sup_bp); \
		sup->su_nbytes -= num << fs->lfs_bshift; \
		LFS_UBWRITE(sup_bp); \
		blocksreleased += num; \
	}

#define SEGDEC { \
	if (daddr != UNASSIGNED) { \
		if (lastseg != (seg = datosn(fs, daddr))) { \
			UPDATE_SEGUSE; \
			num = 1; \
			lastseg = seg; \
		} else \
			++num; \
	} \
}

/*
 * Truncate the inode ip to at most length size.  Update segment usage
 * table information.
 */
/* ARGSUSED */
int
lfs_truncate(ap)
	struct vop_truncate_args /* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register INDIR *inp;
	register int i;
	register daddr_t *daddrp;
	register struct vnode *vp = ap->a_vp;
	off_t length = ap->a_length;
	struct buf *bp, *sup_bp;
	struct ifile *ifp;
	struct inode *ip;
	struct lfs *fs;
	INDIR a[NIADDR + 2], a_end[NIADDR + 2];
	SEGUSE *sup;
	daddr_t daddr, lastblock, lbn, olastblock;
	long off, blocksreleased;
	int e1, e2, depth, lastseg, num, offset, seg, size;

#ifdef VERBOSE
	printf("lfs_truncate\n");
#endif
	vnode_pager_setsize(vp, (u_long)length);

	ip = VTOI(vp);
	fs = ip->i_lfs;

	/* If truncating the file to 0, update the version number. */
	if (length == 0) {
		LFS_IENTRY(ifp, fs, ip->i_number, bp);
		++ifp->if_version;
		LFS_UBWRITE(bp);
	}

	/* If length is larger than the file, just update the times. */
	if (ip->i_size <= length) {
		ip->i_flag |= ICHG|IUPD;
		return (VOP_UPDATE(vp, &time, &time, 1));
	}

	/*
	 * Calculate index into inode's block list of last direct and indirect
	 * blocks (if any) which we want to keep.  Lastblock is 0 when the
	 * file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->lfs_bsize - 1);
	olastblock = lblkno(fs, ip->i_size + fs->lfs_bsize - 1) - 1;

	/*
	 * Update the size of the file. If the file is not being truncated to
	 * a block boundry, the contents of the partial block following the end
	 * of the file must be zero'ed in case it ever become accessable again
	 * because of subsequent file growth.
	 */
	offset = blkoff(fs, length);
	if (offset == 0)
		ip->i_size = length;
	else {
		lbn = lblkno(fs, length);
#ifdef QUOTA
		if (e1 = getinoquota(ip))
			return (e1);
#endif	
		if (e1 = bread(vp, lbn, fs->lfs_bsize, NOCRED, &bp))
			return (e1);
		ip->i_size = length;
		size = blksize(fs);
		(void)vnode_pager_uncache(vp);
		bzero(bp->b_un.b_addr + offset, (unsigned)(size - offset));
		allocbuf(bp, size);
		LFS_UBWRITE(bp);
	}
	/*
	 * Modify sup->su_nbyte counters for each deleted block; keep track
	 * of number of blocks removed for ip->i_blocks.
	 */
	blocksreleased = 0;
	num = 0;
	lastseg = -1;

	for (lbn = olastblock; lbn >= lastblock;) {
		lfs_bmaparray(vp, lbn, &daddr, a, &depth);
		if (lbn == olastblock)
			for (i = NIADDR + 2; i--;)
				a_end[i] = a[i];
		switch (depth) {
		case 0:				/* Direct block. */
			daddr = ip->i_db[lbn];
			SEGDEC;
			ip->i_db[lbn] = 0;
			--lbn;
			break;
#ifdef DIAGNOSTIC
		case 1:				/* An indirect block. */
			panic("lfs_truncate: lfs_bmaparray returned depth 1");
			/* NOTREACHED */
#endif
		default:			/* Chain of indirect blocks. */
			inp = a + --depth;
			if (inp->in_off > 0 && lbn != lastblock) {
				lbn -= inp->in_off < lbn - lastblock ?
				    inp->in_off : lbn - lastblock;
				break;
			}
			for (; depth && (inp->in_off == 0 || lbn == lastblock);
			    --inp, --depth) {
				/*
				 * XXX
				 * The indirect block may not yet exist, so
				 * bread will create one just so we can free
				 * it.
				 */
				if (bread(vp,
				    inp->in_lbn, fs->lfs_bsize, NOCRED, &bp))
					panic("lfs_truncate: bread bno %d",
					    inp->in_lbn);
				daddrp = bp->b_un.b_daddr + inp->in_off;
				for (i = inp->in_off;
				    i++ <= a_end[depth].in_off;) {
					daddr = *daddrp++;
					SEGDEC;
				}
				a_end[depth].in_off = NINDIR(fs) - 1;
				if (inp->in_off == 0)
					brelse (bp);
				else {
					bzero(bp->b_un.b_daddr + inp->in_off,
					    fs->lfs_bsize - 
					    inp->in_off * sizeof(daddr_t));
					LFS_UBWRITE(bp);
				}
			}
			if (depth == 0 && a[1].in_off == 0) {
				off = a[0].in_off;
				daddr = ip->i_ib[off];
				SEGDEC;
				ip->i_ib[off] = 0;
			}
			if (lbn == lastblock || lbn <= NDADDR)
				--lbn;
			else {
				lbn -= NINDIR(fs);
				if (lbn < lastblock)
					lbn = lastblock;
			}
		}
	}
	UPDATE_SEGUSE;
	ip->i_blocks -= blocksreleased;
	/* 
	 * XXX
	 * Currently, we don't know when we allocate an indirect block, so
	 * ip->i_blocks isn't getting incremented appropriately.  As a result,
	 * when we delete any indirect blocks, we get a bad number here.
	 */
	if (ip->i_blocks < 0)
		ip->i_blocks = 0;
	ip->i_flag |= ICHG|IUPD;
	e1 = vinvalbuf(vp, length > 0, ap->a_cred, ap->a_p); 
	e2 = VOP_UPDATE(vp, &time, &time, MNT_WAIT);
	return (e1 ? e1 : e2 ? e2 : 0);
}
