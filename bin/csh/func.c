/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 */

#ifndef lint
static char sccsid[] = "@(#)func.c	5.14 (Berkeley) 04/04/91";
#endif /* not lint */

#include "sh.h"
#include <sys/ioctl.h>
#include "pathnames.h"

/*
 * C shell
 */

struct biltins *
isbfunc(t)
	struct command *t;
{
	register char *cp = t->t_dcom[0];
	register struct biltins *bp, *bp1, *bp2;
	int dolabel(), dofg1(), dobg1();
	static struct biltins label = { "", dolabel, 0, 0 };
	static struct biltins foregnd = { "%job", dofg1, 0, 0 };
	static struct biltins backgnd = { "%job &", dobg1, 0, 0 };

	if (lastchr(cp) == ':') {
		label.bname = cp;
		return (&label);
	}
	if (*cp == '%') {
		if (t->t_dflg & F_AMPERSAND) {
			t->t_dflg &= ~F_AMPERSAND;
			backgnd.bname = cp;
			return (&backgnd);
		}
		foregnd.bname = cp;
		return (&foregnd);
	}
	/*
	 * Binary search
	 * Bp1 is the beginning of the current search range.
	 * Bp2 is one past the end.
	 */
	for (bp1 = bfunc, bp2 = bfunc + nbfunc; bp1 < bp2;) {
		register i;

		bp = bp1 + (bp2 - bp1 >> 1);
		if ((i = *cp - *bp->bname) == 0 &&
		    (i = strcmp(cp, bp->bname)) == 0)
			return bp;
		if (i < 0)
			bp2 = bp;
		else
			bp1 = bp + 1;
	}
	return (0);
}

func(t, bp)
	register struct command *t;
	register struct biltins *bp;
{
	int i;

	xechoit(t->t_dcom);
	setname(bp->bname);
	i = blklen(t->t_dcom) - 1;
	if (i < bp->minargs)
		bferr("Too few arguments");
	if (i > bp->maxargs)
		bferr("Too many arguments");
	(*bp->bfunct)(t->t_dcom, t);
}

dolabel()
{

}

doonintr(v)
	char **v;
{
	register char *cp;
	register char *vv = v[1];

	if (parintr == SIG_IGN)
		return;
	if (setintr && intty)
		bferr("Can't from terminal");
	cp = gointr, gointr = 0, xfree(cp);
	if (vv == 0) {
		if (setintr)
			(void) sigblock(sigmask(SIGINT));
		else
			(void) signal(SIGINT, SIG_DFL);
		gointr = 0;
	} else if (eq((vv = strip(vv)), "-")) {
		(void) signal(SIGINT, SIG_IGN);
		gointr = "-";
	} else {
		gointr = savestr(vv);
		(void) signal(SIGINT, pintr);
	}
}

donohup()
{

	if (intty)
		bferr("Can't from terminal");
	if (setintr == 0) {
		(void) signal(SIGHUP, SIG_IGN);
#ifdef CC
		submit(getpid());
#endif
	}
}

dozip()
{

	;
}

prvars()
{

	plist(&shvhed);
}

doalias(v)
	register char **v;
{
	register struct varent *vp;
	register char *p;

	v++;
	p = *v++;
	if (p == 0)
		plist(&aliases);
	else if (*v == 0) {
		vp = adrof1(strip(p), &aliases);
		if (vp)
			blkpr(vp->vec), printf("\n");
	} else {
		if (eq(p, "alias") || eq(p, "unalias")) {
			setname(p);
			bferr("Too dangerous to alias that");
		}
		set1(strip(p), saveblk(v), &aliases);
	}
}

unalias(v)
	char **v;
{

	unset1(v, &aliases);
}

dologout()
{

	islogin();
	goodbye();
}

dologin(v)
	char **v;
{

	islogin();
	rechist();
	(void) signal(SIGTERM, parterm);
	execl(_PATH_LOGIN, "login", v[1], 0);
	untty();
	exit(1);
}

islogin()
{

	if (chkstop == 0 && setintr)
		panystop(0);
	if (loginsh)
		return;
	error("Not login shell");
}

doif(v, kp)
	char **v;
	struct command *kp;
{
	register int i;
	register char **vv;

	v++;
	i = exp(&v);
	vv = v;
	if (*vv == NOSTR)
		bferr("Empty if");
	if (eq(*vv, "then")) {
		if (*++vv)
			bferr("Improper then");
		setname("then");
		/*
		 * If expression was zero, then scan to else,
		 * otherwise just fall into following code.
		 */
		if (!i)
			search(T_IF, 0);
		return;
	}
	/*
	 * Simple command attached to this if.
	 * Left shift the node in this tree, munging it
	 * so we can reexecute it.
	 */
	if (i) {
		lshift(kp->t_dcom, vv - kp->t_dcom);
		reexecute(kp);
		donefds();
	}
}

/*
 * Reexecute a command, being careful not
 * to redo i/o redirection, which is already set up.
 */
reexecute(kp)
	register struct command *kp;
{

	kp->t_dflg &= F_SAVE;
	kp->t_dflg |= F_REPEAT;
	/*
	 * If tty is still ours to arbitrate, arbitrate it;
	 * otherwise dont even set pgrp's as the jobs would
	 * then have no way to get the tty (we can't give it
	 * to them, and our parent wouldn't know their pgrp, etc.
	 */
	execute(kp, tpgrp > 0 ? tpgrp : -1);
}

doelse()
{

	search(T_ELSE, 0);
}

dogoto(v)
	char **v;
{
	register struct whyle *wp;
	char *lp;

	/*
	 * While we still can, locate any unknown ends of existing loops.
	 * This obscure code is the WORST result of the fact that we
	 * don't really parse.
	 */
	for (wp = whyles; wp; wp = wp->w_next)
		if (wp->w_end == 0) {
			search(T_BREAK, 0);
			wp->w_end = btell();
		} else
			bseek(wp->w_end);
	search(T_GOTO, 0, lp = globone(v[1]));
	xfree(lp);
	/*
	 * Eliminate loops which were exited.
	 */
	wfree();
}

doswitch(v)
	register char **v;
{
	register char *cp, *lp;

	v++;
	if (!*v || *(*v++) != '(')
		goto syntax;
	cp = **v == ')' ? "" : *v++;
	if (*(*v++) != ')')
		v--;
	if (*v)
syntax:
		error("Syntax error");
	search(T_SWITCH, 0, lp = globone(cp));
	xfree(lp);
}

dobreak()
{

	if (whyles)
		toend();
	else
		bferr("Not in while/foreach");
}

doexit(v)
	char **v;
{

	if (chkstop == 0)
		panystop(0);
	/*
	 * Don't DEMAND parentheses here either.
	 */
	v++;
	if (*v) {
		set("status", putn(exp(&v)));
		if (*v)
			bferr("Expression syntax");
	}
	btoeof();
	if (intty)
		(void) close(SHIN);
}

doforeach(v)
	register char **v;
{
	register char *cp;
	register struct whyle *nwp;

	v++;
	cp = strip(*v);
	while (*cp && letter(*cp))
		cp++;
	if (*cp || strlen(*v) >= 20)
		bferr("Invalid variable");
	cp = *v++;
	if (v[0][0] != '(' || v[blklen(v) - 1][0] != ')')
		bferr("Words not ()'ed");
	v++;
	gflag = 0, tglob(v);
	v = globall(v);
	if (v == 0)
		bferr("No match");
	nwp = (struct whyle *) calloc(1, sizeof *nwp);
	nwp->w_fe = nwp->w_fe0 = v; gargv = 0;
	nwp->w_start = btell();
	nwp->w_fename = savestr(cp);
	nwp->w_next = whyles;
	whyles = nwp;
	/*
	 * Pre-read the loop so as to be more
	 * comprehensible to a terminal user.
	 */
	if (intty)
		preread();
	doagain();
}

dowhile(v)
	char **v;
{
	register int status;
	register bool again = whyles != 0 && whyles->w_start == lineloc &&
	    whyles->w_fename == 0;

	v++;
	/*
	 * Implement prereading here also, taking care not to
	 * evaluate the expression before the loop has been read up
	 * from a terminal.
	 */
	if (intty && !again)
		status = !exp0(&v, 1);
	else
		status = !exp(&v);
	if (*v)
		bferr("Expression syntax");
	if (!again) {
		register struct whyle *nwp = (struct whyle *) calloc(1, sizeof (*nwp));

		nwp->w_start = lineloc;
		nwp->w_end = 0;
		nwp->w_next = whyles;
		whyles = nwp;
		if (intty) {
			/*
			 * The tty preread
			 */
			preread();
			doagain();
			return;
		}
	}
	if (status)
		/* We ain't gonna loop no more, no more! */
		toend();
}

preread()
{

	whyles->w_end = -1;
	if (setintr)
		(void) sigsetmask(sigblock(0L) & ~sigmask(SIGINT));
	search(T_BREAK, 0);
	if (setintr)
		(void) sigblock(sigmask(SIGINT));
	whyles->w_end = btell();
}

doend()
{

	if (!whyles)
		bferr("Not in while/foreach");
	whyles->w_end = btell();
	doagain();
}

docontin()
{

	if (!whyles)
		bferr("Not in while/foreach");
	doagain();
}

doagain()
{

	/* Repeating a while is simple */
	if (whyles->w_fename == 0) {
		bseek(whyles->w_start);
		return;
	}
	/*
	 * The foreach variable list actually has a spurious word
	 * ")" at the end of the w_fe list.  Thus we are at the
	 * of the list if one word beyond this is 0.
	 */
	if (!whyles->w_fe[1]) {
		dobreak();
		return;
	}
	set(whyles->w_fename, savestr(*whyles->w_fe++));
	bseek(whyles->w_start);
}

dorepeat(v, kp)
	char **v;
	struct command *kp;
{
	register int i;
	register long	omask;

	i = getn(v[1]);
	if (setintr)
		omask = sigblock(sigmask(SIGINT)) & ~sigmask(SIGINT);
	lshift(v, 2);
	while (i > 0) {
		if (setintr)
			(void) sigsetmask(omask);
		reexecute(kp);
		--i;
	}
	donefds();
	if (setintr)
		(void) sigsetmask(omask);
}

doswbrk()
{

	search(T_BRKSW, 0);
}

srchx(cp)
	register char *cp;
{
	register struct srch *sp, *sp1, *sp2;
	register i;

	/*
	 * Binary search
	 * Sp1 is the beginning of the current search range.
	 * Sp2 is one past the end.
	 */
	for (sp1 = srchn, sp2 = srchn + nsrchn; sp1 < sp2;) {
		sp = sp1 + (sp2 - sp1 >> 1);
		if ((i = *cp - *sp->s_name) == 0 &&
		    (i = strcmp(cp, sp->s_name)) == 0)
			return sp->s_value;
		if (i < 0)
			sp2 = sp;
		else
			sp1 = sp + 1;
	}
	return (-1);
}

char	Stype;
char	*Sgoal;

/*VARARGS2*/
search(type, level, goal)
	int type;
	register int level;
	char *goal;
{
	char wordbuf[BUFSIZ];
	register char *aword = wordbuf;
	register char *cp;

	Stype = type; Sgoal = goal;
	if (type == T_GOTO)
		bseek((off_t)0);
	do {
		if (intty && fseekp == feobp)
			printf("? "), flush();
		aword[0] = 0;
		(void) getword(aword);
		switch (srchx(aword)) {

		case T_ELSE:
			if (level == 0 && type == T_IF)
				return;
			break;

		case T_IF:
			while (getword(aword))
				continue;
			if ((type == T_IF || type == T_ELSE) &&
			    eq(aword, "then"))
				level++;
			break;

		case T_ENDIF:
			if (type == T_IF || type == T_ELSE)
				level--;
			break;

		case T_FOREACH:
		case T_WHILE:
			if (type == T_BREAK)
				level++;
			break;

		case T_END:
			if (type == T_BREAK)
				level--;
			break;

		case T_SWITCH:
			if (type == T_SWITCH || type == T_BRKSW)
				level++;
			break;

		case T_ENDSW:
			if (type == T_SWITCH || type == T_BRKSW)
				level--;
			break;

		case T_LABEL:
			if (type == T_GOTO && getword(aword) && eq(aword, goal))
				level = -1;
			break;

		default:
			if (type != T_GOTO && (type != T_SWITCH || level != 0))
				break;
			if (lastchr(aword) != ':')
				break;
			aword[strlen(aword) - 1] = 0;
			if (type == T_GOTO && eq(aword, goal) ||
			    type == T_SWITCH && eq(aword, "default"))
				level = -1;
			break;

		case T_CASE:
			if (type != T_SWITCH || level != 0)
				break;
			(void) getword(aword);
			if (lastchr(aword) == ':')
				aword[strlen(aword) - 1] = 0;
			cp = strip(Dfix1(aword));
			if (Gmatch(goal, cp))
				level = -1;
			xfree(cp);
			break;

		case T_DEFAULT:
			if (type == T_SWITCH && level == 0)
				level = -1;
			break;
		}
		(void) getword(NOSTR);
	} while (level >= 0);
}

getword(wp)
	register char *wp;
{
	register int found = 0;
	register int c, d;

	c = readc(1);
	d = 0;
	do {
		while (c == ' ' || c == '\t')
			c = readc(1);
		if (c == '#')
			do
				c = readc(1);
			while (c >= 0 && c != '\n');
		if (c < 0)
			goto past;
		if (c == '\n') {
			if (wp)
				break;
			return (0);
		}
		unreadc(c);
		found = 1;
		do {
			c = readc(1);
			if (c == '\\' && (c = readc(1)) == '\n')
				c = ' ';
			if (c == '\'' || c == '"')
				if (d == 0)
					d = c;
				else if (d == c)
					d = 0;
			if (c < 0)
				goto past;
			if (wp)
				*wp++ = c;
		} while ((d || c != ' ' && c != '\t') && c != '\n');
	} while (wp == 0);
	unreadc(c);
	if (found)
		*--wp = 0;
	return (found);

past:
	switch (Stype) {

	case T_IF:
		bferr("then/endif not found");

	case T_ELSE:
		bferr("endif not found");

	case T_BRKSW:
	case T_SWITCH:
		bferr("endsw not found");

	case T_BREAK:
		bferr("end not found");

	case T_GOTO:
		setname(Sgoal);
		bferr("label not found");
	}
	/*NOTREACHED*/
}

toend()
{

	if (whyles->w_end == 0) {
		search(T_BREAK, 0);
		whyles->w_end = btell() - 1;
	} else
		bseek(whyles->w_end);
	wfree();
}

wfree()
{
	long o = btell();

	while (whyles) {
		register struct whyle *wp = whyles;
		register struct whyle *nwp = wp->w_next;

		if (o >= wp->w_start && (wp->w_end == 0 || o < wp->w_end))
			break;
		if (wp->w_fe0)
			blkfree(wp->w_fe0);
		if (wp->w_fename)
			xfree(wp->w_fename);
		xfree((char *)wp);
		whyles = nwp;
	}
}

doecho(v)
	char **v;
{

	echo(' ', v);
}

doglob(v)
	char **v;
{

	echo(0, v);
	flush();
}

echo(sep, v)
	char sep;
	register char **v;
{
	register char *cp;
	int nonl = 0;

	if (setintr)
		(void) sigsetmask(sigblock(0L) & ~sigmask(SIGINT));
	v++;
	if (*v == 0)
		return;
	gflag = 0, tglob(v);
	if (gflag) {
		v = globall(v);
		if (v == 0)
			bferr("No match");
	} else
		trim(v);
	if (sep == ' ' && *v && !strcmp(*v, "-n"))
		nonl++, v++;
	while (cp = *v++) {
		register int c;

		while (c = *cp++)
			cshputchar(c | QUOTE);
		if (*v)
			cshputchar(sep | QUOTE);
	}
	if (sep && nonl == 0)
		cshputchar('\n');
	else
		flush();
	if (setintr)
		(void) sigblock(sigmask(SIGINT));
	if (gargv)
		blkfree(gargv), gargv = 0;
}

char	**environ;

dosetenv(v)
	register char **v;
{
	char *vp, *lp;

	v++;
	if ((vp = *v++) == 0) {
		register char **ep;

		if (setintr)
			(void) sigsetmask(sigblock(0L) & ~ sigmask(SIGINT));
		for (ep = environ; *ep; ep++)
			printf("%s\n", *ep);
		return;
	}
	if ((lp = *v++) == 0)
		lp = "";
	setenv(vp, lp = globone(lp));
	if (eq(vp, "PATH")) {
		importpath(lp);
		dohash();
	}
	xfree(lp);
}

dounsetenv(v)
	register char **v;
{

	v++;
	do
		unsetenv(*v++);
	while (*v);
}

setenv(name, val)
	char *name, *val;
{
	register char **ep = environ;
	register char *cp, *dp;
	char *blk[2], **oep = ep;

	for (; *ep; ep++) {
		for (cp = name, dp = *ep; *cp && *cp == *dp; cp++, dp++)
			continue;
		if (*cp != 0 || *dp != '=')
			continue;
		cp = strspl("=", val);
		xfree(*ep);
		*ep = strspl(name, cp);
		xfree(cp);
		trim(ep);
		return;
	}
	blk[0] = strspl(name, "="); blk[1] = 0;
	environ = blkspl(environ, blk);
	xfree((char *)oep);
	setenv(name, val);
}

unsetenv(name)
	char *name;
{
	register char **ep = environ;
	register char *cp, *dp;
	char **oep = ep;

	for (; *ep; ep++) {
		for (cp = name, dp = *ep; *cp && *cp == *dp; cp++, dp++)
			continue;
		if (*cp != 0 || *dp != '=')
			continue;
		cp = *ep;
		*ep = 0;
		environ = blkspl(environ, ep+1);
		*ep = cp;
		xfree(cp);
		xfree((char *)oep);
		return;
	}
}

doumask(v)
	register char **v;
{
	register char *cp = v[1];
	register int i;

	if (cp == 0) {
		i = umask(0);
		(void) umask(i);
		printf("%o\n", i);
		return;
	}
	i = 0;
	while (digit(*cp) && *cp != '8' && *cp != '9')
		i = i * 8 + *cp++ - '0';
	if (*cp || i < 0 || i > 0777)
		bferr("Improper mask");
	(void) umask(i);
}


struct limits {
	int	limconst;
	char	*limname;
	int	limdiv;
	char	*limscale;
} limits[] = {
	RLIMIT_CPU,	"cputime",	1,	"seconds",
	RLIMIT_FSIZE,	"filesize",	1024,	"kbytes",
	RLIMIT_DATA,	"datasize",	1024,	"kbytes",
	RLIMIT_STACK,	"stacksize",	1024,	"kbytes",
	RLIMIT_CORE,	"coredumpsize",	1024,	"kbytes",
	RLIMIT_RSS,	"memoryuse",	1024,	"kbytes",
	-1,		0,
};

struct limits *
findlim(cp)
	char *cp;
{
	register struct limits *lp, *res;

	res = 0;
	for (lp = limits; lp->limconst >= 0; lp++)
		if (prefix(cp, lp->limname)) {
			if (res)
				bferr("Ambiguous");
			res = lp;
		}
	if (res)
		return (res);
	bferr("No such limit");
	/*NOTREACHED*/
}

dolimit(v)
	register char **v;
{
	register struct limits *lp;
	register int limit;
	char hard = 0;

	v++;
	if (*v && eq(*v, "-h")) {
		hard = 1;
		v++;
	}
	if (*v == 0) {
		for (lp = limits; lp->limconst >= 0; lp++)
			plim(lp, hard);
		return;
	}
	lp = findlim(v[0]);
	if (v[1] == 0) {
		plim(lp,  hard);
		return;
	}
	limit = getval(lp, v+1);
	if (setlim(lp, hard, limit) < 0)
		error(NOSTR);
}

getval(lp, v)
	register struct limits *lp;
	char **v;
{
	register float f;
	double atof();
	char *cp = *v++;

	f = atof(cp);
	while (digit(*cp) || *cp == '.' || *cp == 'e' || *cp == 'E')
		cp++;
	if (*cp == 0) {
		if (*v == 0)
			return ((int)(f+0.5) * lp->limdiv);
		cp = *v;
	}
	switch (*cp) {

	case ':':
		if (lp->limconst != RLIMIT_CPU)
			goto badscal;
		return ((int)(f * 60.0 + atof(cp+1)));

	case 'h':
		if (lp->limconst != RLIMIT_CPU)
			goto badscal;
		limtail(cp, "hours");
		f *= 3600.;
		break;

	case 'm':
		if (lp->limconst == RLIMIT_CPU) {
			limtail(cp, "minutes");
			f *= 60.;
			break;
		}
	case 'M':
		if (lp->limconst == RLIMIT_CPU)
			goto badscal;
		*cp = 'm';
		limtail(cp, "megabytes");
		f *= 1024.*1024.;
		break;

	case 's':
		if (lp->limconst != RLIMIT_CPU)
			goto badscal;
		limtail(cp, "seconds");
		break;

	case 'k':
		if (lp->limconst == RLIMIT_CPU)
			goto badscal;
		limtail(cp, "kbytes");
		f *= 1024;
		break;

	case 'u':
		limtail(cp, "unlimited");
		return (RLIM_INFINITY);

	default:
badscal:
		bferr("Improper or unknown scale factor");
	}
	return ((int)(f+0.5));
}

limtail(cp, str0)
	char *cp, *str0;
{
	register char *str = str0;

	while (*cp && *cp == *str)
		cp++, str++;
	if (*cp)
		error("Bad scaling; did you mean ``%s''?", str0);
}

plim(lp, hard)
	register struct limits *lp;
	char hard;
{
	struct rlimit rlim;
	int limit;

	printf("%s \t", lp->limname);
	(void) getrlimit(lp->limconst, &rlim);
	limit = hard ? rlim.rlim_max : rlim.rlim_cur;
	if (limit == RLIM_INFINITY)
		printf("unlimited");
	else if (lp->limconst == RLIMIT_CPU)
		psecs((long)limit);
	else
		printf("%d %s", limit / lp->limdiv, lp->limscale);
	printf("\n");
}

dounlimit(v)
	register char **v;
{
	register struct limits *lp;
	int err = 0;
	char hard = 0;

	v++;
	if (*v && eq(*v, "-h")) {
		hard = 1;
		v++;
	}
	if (*v == 0) {
		for (lp = limits; lp->limconst >= 0; lp++)
			if (setlim(lp, hard, (int)RLIM_INFINITY) < 0)
				err++;
		if (err)
			error(NOSTR);
		return;
	}
	while (*v) {
		lp = findlim(*v++);
		if (setlim(lp, hard, (int)RLIM_INFINITY) < 0)
			error(NOSTR);
	}
}

setlim(lp, hard, limit)
	register struct limits *lp;
	char hard;
{
	struct rlimit rlim;

	(void) getrlimit(lp->limconst, &rlim);
	if (hard)
		rlim.rlim_max = limit;
  	else if (limit == RLIM_INFINITY && geteuid() != 0)
 		rlim.rlim_cur = rlim.rlim_max;
 	else
 		rlim.rlim_cur = limit;
	if (setrlimit(lp->limconst, &rlim) < 0) {
		printf("%s: %s: Can't %s%s limit\n", bname, lp->limname,
		    limit == RLIM_INFINITY ? "remove" : "set",
		    hard ? " hard" : "");
		return (-1);
	}
	return (0);
}

dosuspend()
{
	sig_t old;
	int ldisc, ctpgrp;

	if (loginsh)
		error("Can't suspend a login shell (yet)");
	untty();
	old = signal(SIGTSTP, SIG_DFL);
	(void) kill(0, SIGTSTP);
	/* the shell stops here */
	(void) signal(SIGTSTP, old);
	if (tpgrp != -1) {
retry:
		(void) ioctl(FSHTTY, TIOCGPGRP, (char *)&ctpgrp);
		if (ctpgrp != opgrp) {
			old = signal(SIGTTIN, SIG_DFL);
			(void) kill(0, SIGTTIN);
			(void) signal(SIGTTIN, old);
			goto retry;
		}
		(void) setpgrp(0, shpgrp);
		(void) ioctl(FSHTTY, TIOCSPGRP, (char *)&shpgrp);
	}
}

doeval(v)
	char **v;
{
	char **oevalvec = evalvec;
	char *oevalp = evalp;
	jmp_buf osetexit;
	int reenter;
	char **gv = 0;

	v++;
	if (*v == 0)
		return;
	gflag = 0, tglob(v);
	if (gflag) {
		gv = v = globall(v);
		gargv = 0;
		if (v == 0)
			error("No match");
		v = copyblk(v);
	} else
		trim(v);
	getexit(osetexit);
	reenter = 0;
	(void)setjmp(reslab);
	reenter++;
	if (reenter == 1) {
		evalvec = v;
		evalp = 0;
		process(0);
	}
	evalvec = oevalvec;
	evalp = oevalp;
	doneinp = 0;
	if (gv)
		blkfree(gv);
	resexit(osetexit);
	if (reenter >= 2)
		error(NOSTR);
}
