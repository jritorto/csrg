/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)remote.c	8.1 (Berkeley) 06/06/93";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>

#include "pathnames.h"
#include "tip.h"

/*
 * Attributes to be gleened from remote host description
 *   data base.
 */
static char **caps[] = {
	&AT, &DV, &CM, &CU, &EL, &IE, &OE, &PN, &PR, &DI,
	&ES, &EX, &FO, &RC, &RE, &PA
};

static char *capstrings[] = {
	"at", "dv", "cm", "cu", "el", "ie", "oe", "pn", "pr",
	"di", "es", "ex", "fo", "rc", "re", "pa", 0
};

static char	*db_array[3] = { _PATH_REMOTE, 0, 0 };

#define cgetflag(f)	(cgetcap(bp, f, ':') != NULL)

static
getremcap(host)
	register char *host;
{
	register char **p, ***q;
	char *bp;
	char *rempath;
	int   stat;

	rempath = getenv("REMOTE");
	if (rempath != NULL)
		if (*rempath != '/')
			/* we have an entry */
			cgetset(rempath);
		else {	/* we have a path */
			db_array[1] = rempath;
			db_array[2] = _PATH_REMOTE;
		}

	if ((stat = cgetent(&bp, db_array, host)) < 0) {
		if (DV ||
		    host[0] == '/' && access(DV = host, R_OK | W_OK) == 0) {
			CU = DV;
			HO = host;
			HW = 1;
			DU = 0;
			if (!BR)
				BR = DEFBR;
			FS = DEFFS;
			return;
		}
		switch(stat) {
		case -1:
			fprintf(stderr, "tip: unknown host %s\n", host);
			break;
		case -2:
			fprintf(stderr, 
			    "tip: can't open host description file\n");
			break;
		case -3:
			fprintf(stderr, 
			    "tip: possible reference loop in host description file\n");
			break;
		}
		exit(3);
	}

	for (p = capstrings, q = caps; *p != NULL; p++, q++)
		if (**q == NULL)
			cgetstr(bp, *p, *q);
	if (!BR && (cgetnum(bp, "br", &BR) == -1))
		BR = DEFBR;
	if (cgetnum(bp, "fs", &FS) == -1)
		FS = DEFFS;
	if (DU < 0)
		DU = 0;
	else
		DU = cgetflag("du");
	if (DV == NOSTR) {
		fprintf(stderr, "%s: missing device spec\n", host);
		exit(3);
	}
	if (DU && CU == NOSTR)
		CU = DV;
	if (DU && PN == NOSTR) {
		fprintf(stderr, "%s: missing phone number\n", host);
		exit(3);
	}

	HD = cgetflag("hd");

	/*
	 * This effectively eliminates the "hw" attribute
	 *   from the description file
	 */
	if (!HW)
		HW = (CU == NOSTR) || (DU && equal(DV, CU));
	HO = host;
	/*
	 * see if uppercase mode should be turned on initially
	 */
	if (cgetflag("ra"))
		boolean(value(RAISE)) = 1;
	if (cgetflag("ec"))
		boolean(value(ECHOCHECK)) = 1;
	if (cgetflag("be"))
		boolean(value(BEAUTIFY)) = 1;
	if (cgetflag("nb"))
		boolean(value(BEAUTIFY)) = 0;
	if (cgetflag("sc"))
		boolean(value(SCRIPT)) = 1;
	if (cgetflag("tb"))
		boolean(value(TABEXPAND)) = 1;
	if (cgetflag("vb"))
		boolean(value(VERBOSE)) = 1;
	if (cgetflag("nv"))
		boolean(value(VERBOSE)) = 0;
	if (cgetflag("ta"))
		boolean(value(TAND)) = 1;
	if (cgetflag("nt"))
		boolean(value(TAND)) = 0;
	if (cgetflag("rw"))
		boolean(value(RAWFTP)) = 1;
	if (cgetflag("hd"))
		boolean(value(HALFDUPLEX)) = 1;
	if (RE == NOSTR)
		RE = (char *)"tip.record";
	if (EX == NOSTR)
		EX = (char *)"\t\n\b\f";
	if (ES != NOSTR)
		vstring("es", ES);
	if (FO != NOSTR)
		vstring("fo", FO);
	if (PR != NOSTR)
		vstring("pr", PR);
	if (RC != NOSTR)
		vstring("rc", RC);
	if (cgetnum(bp, "dl", &DL) == -1)
		DL = 0;
	if (cgetnum(bp, "cl", &CL) == -1)
		CL = 0;
	if (cgetnum(bp, "et", &ET) == -1)
		ET = 10;
}

char *
getremote(host)
	char *host;
{
	register char *cp;
	static char *next;
	static int lookedup = 0;

	if (!lookedup) {
		if (host == NOSTR && (host = getenv("HOST")) == NOSTR) {
			fprintf(stderr, "tip: no host specified\n");
			exit(3);
		}
		getremcap(host);
		next = DV;
		lookedup++;
	}
	/*
	 * We return a new device each time we're called (to allow
	 *   a rotary action to be simulated)
	 */
	if (next == NOSTR)
		return (NOSTR);
	if ((cp = index(next, ',')) == NULL) {
		DV = next;
		next = NOSTR;
	} else {
		*cp++ = '\0';
		DV = next;
		next = cp;
	}
	return (DV);
}
