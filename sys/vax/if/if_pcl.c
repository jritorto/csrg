/*	if_pcl.c	4.1	83/03/31	*/

#include "pcl.h"
#if NPCL > 0
/*
 * DEC CSS PCL-11B Parallel Communications Interface
 *
 * Written by Mike Muuss.
 */
#include "../machine/pte.h"

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/mbuf.h"
#include "../h/buf.h"
#include "../h/protosw.h"
#include "../h/socket.h"
#include "../h/vmmac.h"
#include "../h/errno.h"

#include "../net/if.h"
#include "../net/netisr.h"
#include "../net/route.h"
#include "../netinet/in.h"
#include "../netinet/in_systm.h"
#include "../netinet/ip.h"
#include "../netinet/ip_var.h"

#include "../vax/cpu.h"
#include "../vax/mtpr.h"
#include "../vaxif/if_pclreg.h"
#include "../vaxif/if_uba.h"
#include "../vaxuba/ubareg.h"
#include "../vaxuba/ubavar.h"

/* The MTU has been carefully selected to prevent fragmentation <-> ArpaNet */
#define	PCLMTU	(1006)		/* Max transmission unit (bytes) */

int	pclprobe(), pclattach(), pclrint(), pclxint();
int	pclinit(),pcloutput(),pclreset();

struct	uba_device	*pclinfo[NPCL];
u_short pclstd[] = { 0 };
#define	PCLUNIT(x)	minor(x)
struct	uba_driver pcldriver =
	{ pclprobe, 0, pclattach, 0, pclstd, "pcl", pclinfo };

/*
 * PCL software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * sc_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 * We also have, for each interface, a UBA interface structure, which
 * contains information about the UNIBUS resources held by the interface:
 * map registers, buffered data paths, etc.  Information is cached in this
 * structure for use by the if_uba.c routines in running the interface
 * efficiently.
 */
struct	pcl_softc {
	struct	ifnet sc_if;		/* network-visible interface */
	struct	ifuba sc_ifuba;		/* UNIBUS resources */
	short	sc_oactive;		/* is output active? */
	short	sc_olen;		/* length of last output */
	short	sc_lastdest;		/* previous destination */
	short	sc_odest;		/* current xmit destination */
	short	sc_pattern;		/* identification pattern */
} pcl_softc[NPCL];

/*
 * Structure of "local header", which only goes between
 * pcloutput and pclstart.
 */
struct pcl_header {
	short	pcl_dest;		/* Destination PCL station */
};

/*
 * Do non-DMA output of 1 word to determine presence of interface,
 * and to find the interupt vector.  1 word messages are a special
 * case in the receiver routine, and will be discarded.
 */
pclprobe(reg)
	caddr_t reg;
{
	register int br, cvec;		/* r11, r10 value-result */
	register struct pcldevice *addr = (struct pcldevice *)reg;

#ifdef lint
	br = 0; cvec = br; br = cvec;
	pclrint(0); pclxint(0);
#endif
	addr->pcl_rcr = PCL_RCINIT;
	addr->pcl_tcr = PCL_TXINIT;
	addr->pcl_tsba = 0xFFFE;
	/* going for 01777776 */
	addr->pcl_tsbc = -4;		/* really short */
	addr->pcl_tcr =
	 ((1 & 0xF) << 8) | PCL_TXNPR | PCL_SNDWD | PCL_STTXM | PCL_IE | 0x0030;
	DELAY(100000);
	addr->pcl_tcr = PCL_TXINIT;
	return (sizeof (struct pcldevice));
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
pclattach(ui)
	struct uba_device *ui;
{
	register struct pcl_softc *sc = &pcl_softc[ui->ui_unit];
	register struct sockaddr_in *sin;

	sc->sc_if.if_unit = ui->ui_unit;
	sc->sc_if.if_name = "pcl";
	sc->sc_if.if_mtu = PCLMTU;
	/* copy network addr from flags long */
	sc->sc_if.if_net = in_netof(htonl(ui->ui_flags));
	sc->sc_if.if_host[0] = in_lnaof(htonl(ui->ui_flags));

	sin = (struct sockaddr_in *)&sc->sc_if.if_addr;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(ui->ui_flags);

	sc->sc_if.if_init = pclinit;
	sc->sc_if.if_output = pcloutput;
	sc->sc_if.if_reset = pclreset;
	sc->sc_ifuba.ifu_flags = UBA_NEEDBDP;
	if_attach(&sc->sc_if);
}

/*
 * Reset of interface after UNIBUS reset.
 * If interface is on specified uba, reset its state.
 */
pclreset(unit, uban)
	int unit, uban;
{
	register struct uba_device *ui;

	if (unit >= NPCL || (ui = pclinfo[unit]) == 0 || ui->ui_alive == 0 ||
	    ui->ui_ubanum != uban)
		return;
	printf(" pcl%d", unit);
	pclinit(unit);
}

/*
 * Initialization of interface; clear recorded pending
 * operations, and reinitialize UNIBUS usage.
 */
pclinit(unit)
	int unit;
{
	register struct pcl_softc *sc = &pcl_softc[unit];
	register struct uba_device *ui = pclinfo[unit];
	register struct pcldevice *addr;
	int s;

	if (if_ubainit(&sc->sc_ifuba, ui->ui_ubanum, 0,
	    (int)btoc(PCLMTU)) == 0) { 
		printf("pcl%d: can't init\n", unit);
		sc->sc_if.if_flags &= ~IFF_UP;
		return;
	}
	addr = (struct pcldevice *)ui->ui_addr;
	addr->pcl_rcr = PCL_RCINIT;
	addr->pcl_tcr = PCL_TXINIT;

	/*
	 * Hang a receive and start any
	 * pending writes by faking a transmit complete.
	 */
	s = splimp();
	addr->pcl_rdba = (short) sc->sc_ifuba.ifu_r.ifrw_info;
	addr->pcl_rdbc = -PCLMTU;
	addr->pcl_rcr = (((int)(sc->sc_ifuba.ifu_r.ifrw_info>>12))&0x0030) |
		PCL_RCNPR | PCL_RCVWD | PCL_RCVDAT | PCL_IE;
	sc->sc_oactive = 0;
	sc->sc_if.if_flags |= IFF_UP;		/* Mark interface up */
	pclstart(unit);
	splx(s);
	/* Set up routing table entry */
	if_rtinit(&sc->sc_if, RTF_UP);
}

/*
 * PCL output routine.
 */
pcloutput(ifp, m, dst)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
{
	int type, dest, s, error;
	struct pcl_header *pclp;
	struct mbuf *m2;

	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		dest = ((struct sockaddr_in *)dst)->sin_addr.s_addr;
		dest = ntohl(dest);	/* ??? */
		dest = dest & 0xff;
		break;
#endif
	default:
		printf("pcl%d: can't handle af%d\n", ifp->if_unit,
			dst->sa_family);
		error = EAFNOSUPPORT;
		goto bad;
	}

	/*
	 * Add pseudo local net header.
	 * Actually, it does not get transmitted, but merely stripped
	 * off and used by the START routine to route the packet.
	 * If no space in first mbuf, allocate another.
	 */
	if (m->m_off > MMAXOFF ||
	    MMINOFF + sizeof (struct pcl_header) > m->m_off) {
		m2 = m_get(M_DONTWAIT, MT_HEADER);
		if (m2 == 0) {
			error = ENOBUFS;
			goto bad;
		}
		m2->m_next = m;
		m2->m_off = MMINOFF;
		m2->m_len = sizeof (struct pcl_header);
		m = m2;
	} else {
		m->m_off -= sizeof (struct pcl_header);
		m->m_len += sizeof (struct pcl_header);
	}
	pclp = mtod(m, struct pcl_header *);
	pclp->pcl_dest = dest;

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	s = splimp();
	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto qfull;
	}
	IF_ENQUEUE(&ifp->if_snd, m);
	if (pcl_softc[ifp->if_unit].sc_oactive == 0)
		pclstart(ifp->if_unit);
	splx(s);
	return (0);
qfull:
	splx(s);
bad:
	m_freem(m);
	return (error);
}

/*
 * Start or restart output on interface.
 * If interface is already active, then this is a retransmit.
 * If interface is not already active, get another datagram
 * to send off of the interface queue, and map it to the interface
 * before starting the output.
 */
pclstart(dev)
	dev_t dev;
{
        int unit = PCLUNIT(dev);
	struct uba_device *ui = pclinfo[unit];
	register struct pcl_softc *sc = &pcl_softc[unit];
	register struct pcldevice *addr;
	struct mbuf *m;

	if (sc->sc_oactive)
		goto restart;

	/*
	 * Not already active: dequeue another request
	 * and map it to the UNIBUS.  If no more requests,
	 * just return.
	 */
	IF_DEQUEUE(&sc->sc_if.if_snd, m);
	if (m == 0) {
		sc->sc_oactive = 0;
		return;
	}

	/*
	 * Pull destination node out of pseudo-local net header.
	 * remove it from outbound data.
	 * Note that if_wubaput calls m_bcopy, which is prepared for
	 * m_len to be 0 in the first mbuf in the chain.
	 */
	sc->sc_odest = mtod(m, struct pcl_header *)->pcl_dest;
	m->m_off += sizeof (struct pcl_header);
	m->m_len -= sizeof (struct pcl_header);

	/* Map out to the DMA area */
	sc->sc_olen = if_wubaput(&sc->sc_ifuba, m);

restart:
	/*
	 * Have request mapped to UNIBUS for transmission.
	 * Purge any stale data from this BDP, and start the otput.
	 */
	if (sc->sc_ifuba.ifu_flags & UBA_NEEDBDP)
		UBAPURGE(sc->sc_ifuba.ifu_uba, sc->sc_ifuba.ifu_w.ifrw_bdp);
	addr = (struct pcldevice *)ui->ui_addr;
	addr->pcl_tcr = PCL_TXINIT;
	addr->pcl_tsba = (int)sc->sc_ifuba.ifu_w.ifrw_info;
	addr->pcl_tsbc = -sc->sc_olen;

	/*
	 * RIB (retry if busy) is used on the second and subsequent packets
	 * to a single host, because TCP often wants to transmit multiple
	 * buffers in a row,
	 * and if they are all going to the same place, the second and
	 * subsequent ones may be lost due to receiver not ready again yet.
	 * This can cause serious problems, because the TCP will resend the
	 * whole window, which just repeats the problem.  The result is that
	 * a perfectly good link appears not to work unless we take steps here.
	 */
	addr->pcl_tcr = (((int)(sc->sc_ifuba.ifu_w.ifrw_info>>12))&0x0030) |
		((sc->sc_odest & 0xF)<<8) |
		PCL_TXNPR | PCL_SNDWD | PCL_STTXM | PCL_IE |
		(sc->sc_odest == sc->sc_lastdest ? PCL_RIB : 0);
	sc->sc_lastdest = sc->sc_odest;
	sc->sc_oactive = 1;
}

/*
 * PCL transmitter interrupt.
 * Start another output if more data to send.
 */
pclxint(unit)
	int unit;
{
	register struct uba_device *ui = pclinfo[unit];
	register struct pcl_softc *sc = &pcl_softc[unit];
	register struct pcldevice *addr = (struct pcldevice *)ui->ui_addr;

	if (sc->sc_oactive == 0) {
		printf ("pcl%d: stray interrupt\n", unit);
		return;
	}
	if (addr->pcl_tsr & PCL_ERR) {
		sc->sc_lastdest = 0;		/* don't bother with RIB */
		if (addr->pcl_tsr & PCL_MSTDWN) {
			addr->pcl_tmmr = PCL_MASTER|PCL_AUTOADDR;
			pclstart(unit);	/* Retry */
			printf("pcl%d: master\n", unit );
			return;
		}
		if (addr->pcl_tsr & PCL_RESPB) {
			/* Log as an error */
			printf("pcl%d: send error, tcr=%b tsr=%b\n",
				unit, addr->pcl_tcr, PCL_TCSRBITS,
				addr->pcl_tsr, PCL_TERRBITS);
			sc->sc_if.if_oerrors++;
		}
	} else
		sc->sc_if.if_opackets++;
	sc->sc_oactive = 0;
	if (sc->sc_ifuba.ifu_xtofree) {
		m_freem(sc->sc_ifuba.ifu_xtofree);
		sc->sc_ifuba.ifu_xtofree = 0;
	}
	pclstart(unit);
}

/*
 * PCL interface receiver interrupt.
 * If input error just drop packet.
 */
pclrint(unit)
	int unit;
{
	register struct pcl_softc *sc = &pcl_softc[unit];
	struct pcldevice *addr = (struct pcldevice *)pclinfo[unit]->ui_addr;
    	struct mbuf *m;
	int len, plen; short resid;
	register struct ifqueue *inq;
	int off;

	sc->sc_if.if_ipackets++;
	/*
	 * Purge BDP; drop if input error indicated.
	 */
	if (sc->sc_ifuba.ifu_flags & UBA_NEEDBDP)
		UBAPURGE(sc->sc_ifuba.ifu_uba, sc->sc_ifuba.ifu_r.ifrw_bdp);
	if (addr->pcl_rsr & PCL_ERR) {
		printf("pcl%d: rcv error, rcr=%b rsr=%b\n",
			unit, addr->pcl_rcr, PCL_RCSRBITS,
			addr->pcl_rsr, PCL_RERRBITS);
		sc->sc_if.if_ierrors++;
		goto setup;
	}
	len = PCLMTU + addr->pcl_rdbc;
	if (len <= 0 || len > PCLMTU) {
		printf("pcl%d: bad len=%d.\n", unit, len);
		sc->sc_if.if_ierrors++;
		goto setup;
	}

	/* Really short packets will be part of the startup sequence */
	if (len <= 4) {
		/* Later, do comming-up processing here */
		goto setup;	/* drop packet */
	}

	/*
	 * Pull packet off interface.
	 */
	m = if_rubaget(&sc->sc_ifuba, len, 0);
	if (m == 0)
		goto setup;

	schednetisr(NETISR_IP);
	inq = &ipintrq;

	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
setup:
	/*
	 * Reset for next packet.
	 */
	addr->pcl_rcr = PCL_RCINIT;
	addr->pcl_rdba = (int)sc->sc_ifuba.ifu_r.ifrw_info;
	addr->pcl_rdbc = -PCLMTU;
	addr->pcl_rcr = (((int)(sc->sc_ifuba.ifu_w.ifrw_info>>12))&0x0030) |
		PCL_RCNPR | PCL_RCVWD | PCL_RCVDAT | PCL_IE;
}
#endif
