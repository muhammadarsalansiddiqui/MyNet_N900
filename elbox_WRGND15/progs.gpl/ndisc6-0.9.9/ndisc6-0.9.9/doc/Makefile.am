# Makefile.am - doc Makefile for ndisc6
# $Id: Makefile.am 628 2009-08-04 18:37:13Z remi $

# Copyright © 2006-2007 Rémi Denis-Courmont
# This file is distributed under the same license as the ndisc6 package.

man1_MANS = addr2name.1 dnssort.1 tcpspray.1
man8_MANS = ndisc6.8 rdisc6.8 rltraceroute6.8 rdnssd.8 isatapd.8
SOURCES_MAN = \
	ndisc6.8 rdisc6.8 \
	rltraceroute6.8 \
	rdnssd.8-in \
	isatapd.8 \
	$(man1_MANS)

EXTRA_DIST = $(SOURCES_MAN)
CLEANFILES = rdnssd.8

edit = sed \
	-e 's,@localstatedir\@,$(localstatedir),g' \
	-e 's,@sysconfdir\@,$(sysconfdir),g'

.8-in.8:
	rm -f -- $@
	$(edit) $< > $@

install-data-hook:
	inst=`echo rltraceroute6 | sed '$(transform)'`; \
	cd "$(DESTDIR)$(man8dir)" && \
	for a in tcptraceroute6 tracert6; do \
		ln -sf -- "$${inst}.8" "$$a.8" ; \
	done
	inst=`echo tcpspray | sed '$(transform)'`; \
	cd "$(DESTDIR)$(man1dir)" && \
	ln -sf -- "$${inst}.1" "tcpspray6.1"
	inst=`echo addr2name | sed '$(transform)'`; \
	cd "$(DESTDIR)$(man1dir)" && \
	ln -sf -- "$${inst}.1" "name2addr.1"

uninstall-hook:
	for a in tcptraceroute6 tracert6; do \
		unlink "$(DESTDIR)$(man8dir)/$$a.8" ; \
	done
	unlink "$(DESTDIR)$(man1dir)/tcpspray6.1"
	unlink "$(DESTDIR)$(man1dir)/name2addr.1"

# Make sure manpages are plain ASCII
distcheck-hook:
	cd "$(srcdir)" && \
	cat $(SOURCES_MAN) | grep -ve '\.\\"' | \
		iconv -f ASCII >/dev/null
