#	@(#)Makefile	8.2 (Berkeley) 05/23/94

# The following modules do not build/install:
# 10.gdb, 13.rcs

# Missing:  00.contents 01.Clang 02.f77 03.f77io 05.as 09.lint 10.adb 11.dbx
#	    12.make 15.yacc 16.lex 17.m4

BINDIR=	/usr/share/doc/psd
FILES=	Makefile Title
SUBDIR=	04.pascal 06.sysman 07.ipctut \
	08.ipc 13.rcs 14.sccs \
	18.curses

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}

.include <bsd.subdir.mk>
