#	@(#)Makefile	8.3 (Berkeley) 05/08/95

PROG=	df
SRCS=	df.c vfslist.c
BINGRP=	operator
BINMODE=2555

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
