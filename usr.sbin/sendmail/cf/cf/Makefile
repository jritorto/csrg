#	@(#)Makefile	8.14 (Berkeley) 05/29/95

#
#  This Makefile uses the new Berkeley "make" program.  See Makefile.dist
#  for a more vanilla version.
#
#  Configuration files are created using "m4 file.mc > file.cf";
#  this may be easier than tweaking the Makefile.  You do need to
#  have a fairly modern M4 available (GNU m4 works).  On SunOS, use
#  /usr/5bin/m4.
#

M4=	m4
#M4=	/usr/src/usr.bin/m4/obj/m4
CHMOD=	chmod
ROMODE=	444
RM=	rm -f

.SUFFIXES:  .mc .cf

.mc.cf:
	$(RM) $@
	(cd ${.CURDIR} && $(M4) ${@:R}.mc > obj/$@)
	$(CHMOD) $(ROMODE) $@

ALL=	generic-hpux9.cf generic-osf1.cf generic-solaris2.cf \
		generic-sunos4.1.cf generic-ultrix4.cf \
	cs-hpux9.cf cs-osf1.cf cs-solaris2.cf \
		cs-sunos4.1.cf cs-ultrix4.cf \
	s2k-osf1.cf s2k-ultrix4.cf \
	chez.cs.cf huginn.cs.cf mail.cs.cf mail.eecs.cf mailspool.cs.cf \
		python.cs.cf ucbarpa.cf ucbvax.cf vangogh.cs.cf

all: $(ALL)

clean cleandir:
	$(RM) $(ALL) core

depend install: 

# this is overkill, but....
M4FILES=\
	../domain/Berkeley.EDU.m4 \
	../domain/CS.Berkeley.EDU.m4 \
	../domain/EECS.Berkeley.EDU.m4 \
	../domain/S2K.Berkeley.EDU.m4 \
	../feature/allmasquerade.m4 \
	../feature/always_add_domain.m4 \
	../feature/bestmx_is_local.m4 \
	../feature/bitdomain.m4 \
	../feature/domaintable.m4 \
	../feature/local_procmail.m4 \
	../feature/mailertable.m4 \
	../feature/nocanonify.m4 \
	../feature/nodns.m4 \
	../feature/notsticky.m4 \
	../feature/nouucp.m4 \
	../feature/nullclient.m4 \
	../feature/redirect.m4 \
	../feature/smrsh.m4 \
	../feature/stickyhost.m4 \
	../feature/use_cw_file.m4 \
	../feature/uucpdomain.m4 \
	../hack/cssubdomain.m4 \
	../m4/cf.m4 \
	../m4/nullrelay.m4 \
	../m4/proto.m4 \
	../m4/version.m4 \
	../mailer/fax.m4 \
	../mailer/local.m4 \
	../mailer/mail11.m4 \
	../mailer/pop.m4 \
	../mailer/procmail.m4 \
	../mailer/smtp.m4 \
	../mailer/usenet.m4 \
	../mailer/uucp.m4 \
	../ostype/aix3.m4 \
	../ostype/amdahl-uts.m4 \
	../ostype/aux.m4 \
	../ostype/bsd4.3.m4 \
	../ostype/bsd4.4.m4 \
	../ostype/bsdi1.0.m4 \
	../ostype/dgux.m4 \
	../ostype/domainos.m4 \
	../ostype/dynix3.2.m4 \
	../ostype/hpux9.m4 \
	../ostype/irix.m4 \
	../ostype/linux.m4 \
	../ostype/nextstep.m4 \
	../ostype/osf1.m4 \
	../ostype/ptx2.m4 \
	../ostype/riscos4.5.m4 \
	../ostype/sco3.2.m4 \
	../ostype/solaris2.m4 \
	../ostype/sunos3.5.m4 \
	../ostype/sunos4.1.m4 \
	../ostype/svr4.m4 \
	../ostype/ultrix4.m4 \
	../siteconfig/uucp.cogsci.m4 \
	../siteconfig/uucp.old.arpa.m4 \
	../siteconfig/uucp.ucbarpa.m4 \
	../siteconfig/uucp.ucbvax.m4 \

$(ALL):  $(M4FILES)

.include <bsd.prog.mk>
