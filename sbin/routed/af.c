/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)af.c	5.3 (Berkeley) 02/23/86";
#endif not lint

#include "defs.h"

/*
 * Address family support routines
 */
int	inet_hash(), inet_netmatch(), inet_output(),
	inet_portmatch(), inet_portcheck(),
	inet_checkhost(), inet_ishost(), inet_canon();
char	*inet_format();
#define NIL	{ 0 }
#define	INET \
	{ inet_hash,		inet_netmatch,		inet_output, \
	  inet_portmatch,	inet_portcheck,		inet_checkhost, \
	  inet_ishost,		inet_canon, 		inet_format }

struct afswitch afswitch[AF_MAX] =
	{ NIL, NIL, INET, };

int af_max = sizeof(afswitch) / sizeof(afswitch[0]);

struct sockaddr_in inet_default = { AF_INET, INADDR_ANY };

inet_hash(sin, hp)
	register struct sockaddr_in *sin;
	struct afhash *hp;
{
	register u_long n;

	n = inet_netof(sin->sin_addr);
	if (n)
	    while ((n & 0xff) == 0)
		n >>= 8;
	hp->afh_nethash = n;
	hp->afh_hosthash = ntohl(sin->sin_addr.s_addr);
	hp->afh_hosthash &= 0x7fffffff;
}

inet_netmatch(sin1, sin2)
	struct sockaddr_in *sin1, *sin2;
{

	return (inet_netof(sin1->sin_addr) == inet_netof(sin2->sin_addr));
}

/*
 * Verify the message is from the right port.
 */
inet_portmatch(sin)
	register struct sockaddr_in *sin;
{
	
	return (sin->sin_port == sp->s_port);
}

/*
 * Verify the message is from a "trusted" port.
 */
inet_portcheck(sin)
	struct sockaddr_in *sin;
{

	return (ntohs(sin->sin_port) <= IPPORT_RESERVED);
}

/*
 * Internet output routine.
 */
inet_output(s, flags, sin, size)
	int s, flags;
	struct sockaddr_in *sin;
	int size;
{
	struct sockaddr_in dst;

	dst = *sin;
	sin = &dst;
	if (sin->sin_port == 0)
		sin->sin_port = sp->s_port;
	if (sendto(s, packet, size, flags, sin, sizeof (*sin)) < 0)
		perror("sendto");
}

/*
 * Return 1 if the address is believed
 * for an Internet host -- THIS IS A KLUDGE.
 */
inet_checkhost(sin)
	struct sockaddr_in *sin;
{
	u_long i = ntohl(sin->sin_addr.s_addr);

#define	IN_BADCLASS(i)	(((long) (i) & 0xe0000000) == 0xe0000000)

	if (IN_BADCLASS(i) || sin->sin_port != 0)
		return (0);
	if (i != 0 && (i & 0xff000000) == 0)
		return (0);
	for (i = 0; i < sizeof(sin->sin_zero)/sizeof(sin->sin_zero[0]); i++)
		if (sin->sin_zero[i])
			return (0);
	return (1);
}

/*
 * Return 1 if the address is
 * for an Internet host, 0 for a network.
 */
inet_ishost(sin)
	struct sockaddr_in *sin;
{

	return (inet_lnaof(sin->sin_addr) != 0);
}

inet_canon(sin)
	struct sockaddr_in *sin;
{

	sin->sin_port = 0;
}

char *
inet_format(sin)
	struct sockaddr_in *sin;
{
	char *inet_ntoa();

	return (inet_ntoa(sin->sin_addr));
}
