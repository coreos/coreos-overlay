# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-misc/ddccontrol/ddccontrol-0.4.2.ebuild,v 1.8 2010/01/01 18:08:47 ssuominen Exp $

inherit eutils autotools

DESCRIPTION="DDCControl allows control of monitor parameters via DDC"
HOMEPAGE="http://ddccontrol.sourceforge.net/"
SRC_URI="mirror://sourceforge/${PN}/${P}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 ppc x86"
IUSE="gtk gnome doc nls ddcpci"

RDEPEND="dev-libs/libxml2
	gtk? ( >=x11-libs/gtk+-2.4 )
	gnome? ( >=gnome-base/gnome-panel-2.10 )
	sys-apps/pciutils
	nls? ( sys-devel/gettext )
	>=app-misc/ddccontrol-db-20060730"
DEPEND="${RDEPEND}
	dev-perl/XML-Parser
	dev-util/intltool
	doc? ( >=app-text/docbook-xsl-stylesheets-1.65.1
		   >=dev-libs/libxslt-1.1.6
	       app-text/htmltidy )
	sys-kernel/linux-headers"

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/${P}-pciutils-libz.patch

	# Fix sandbox violation
	for i in Makefile.am Makefile.in; do
		sed -i.orig "${S}/src/gddccontrol/${i}" \
		-e "/@INSTALL@/s/ \$(datadir)/ \$(DESTDIR)\/\$(datadir)/" \
		|| die "Failed to fix DESTDIR"
	done

	# ppc/ppc64 do not have inb/outb/ioperm
	# they also do not have (sys|asm)/io.h
	if [ "${ARCH/64}" == "ppc" ]; then
		for card in sis intel810 ; do
			sed -r -i \
				-e "/${card}.Po/d" \
				-e "s~${card}[^[:space:]]*~ ~g" \
				src/ddcpci/Makefile.in
			sed -r -i \
				-e "/${card}.Po/d" \
				-e "s~${card}[^[:space:]]*~ ~g" \
				src/ddcpci/Makefile.am
		done
		sed -i \
			-e '/sis_/d' \
			-e '/i810_/d' \
			src/ddcpci/main.c
	fi

	## Save for a rainy day or future patching
	eautoreconf || die "eautoreconf failed"
	intltoolize --force || die "intltoolize failed"
}

src_compile() {
	econf $(use_enable doc) \
		$(use_enable gtk gnome) \
		$(use_enable gnome gnome-applet) \
		$(use_enable nls) \
		$(use_enable ddcpci) || die "econf failed"
	emake || die "emake failed"
}

src_install() {
	make DESTDIR="${D}" htmldir="/usr/share/doc/${PF}/html" install || die
	dodoc AUTHORS ChangeLog NEWS README TODO
}
