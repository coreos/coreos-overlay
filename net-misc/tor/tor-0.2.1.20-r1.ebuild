# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/tor/tor-0.2.1.20-r1.ebuild,v 1.1 2009/12/06 14:36:45 fauli Exp $

EAPI=2

inherit eutils

DESCRIPTION="Anonymizing overlay network for TCP"
HOMEPAGE="http://www.torproject.org/"
MY_PV=${PV/_/-}
SRC_URI="http://www.torproject.org/dist/${PN}-${MY_PV}.tar.gz"
S="${WORKDIR}/${PN}-${MY_PV}"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~ppc ~ppc64 ~sparc ~x86 ~x86-fbsd"
IUSE="debug"

DEPEND="dev-libs/openssl
	>=dev-libs/libevent-1.2"
RDEPEND="${DEPEND}
	net-proxy/tsocks"

pkg_setup() {
	enewgroup tor
	enewuser tor -1 -1 /var/lib/tor tor
}

src_prepare() {
	epatch "${FILESDIR}"/torrc.sample-0.1.2.6.patch
	epatch "${FILESDIR}"/${PN}-0.2.1.19-logrotate.patch
	epatch "${FILESDIR}"/${PN}-0.2.1.19-openssl.patch
	# Normally tor uses a bundled libevent fragment to provide
	# asynchronous DNS requests.  This is generally a bad idea, but at
	# the moment the official libevent does not have the 0x20 hack, so
	# anonymity is higher with the bundled variant.  Remove patch as
	# soon as upstream has installed the autoconf option to use
	# system's libevent.  This hasn't happened, so we
	# have to live with the bundled libevent for this release, as the
	# current version in tree won't suffice for tor to build
	# See http://bugs.noreply.org/flyspray/index.php?do=details&id=920
	# for upstream's report
	# Let's revisit this when libevent-2* is unmasked
	# use bundledlibevent || epatch "${FILESDIR}"/${PN}-0.2.1.5-no-internal-libevent.patch
}

src_configure() {
	econf $(use_enable debug)
}

src_install() {
	newinitd "${FILESDIR}"/tor.initd-r4 tor
	emake DESTDIR="${D}" install || die
	keepdir /var/{lib,log,run}/tor

	dodoc README ChangeLog AUTHORS ReleaseNotes \
		doc/{HACKING,TODO} \
		doc/spec/*.txt

	fperms 750 /var/lib/tor /var/log/tor
	fperms 755 /var/run/tor
	fowners tor:tor /var/lib/tor /var/log/tor /var/run/tor

	insinto /etc/logrotate.d
	newins contrib/tor.logrotate tor

	# allow the tor user more open files to avoid errors, see bug 251171
	insinto /etc/security/limits.d/
	doins "${FILESDIR}"/tor.conf
}

pkg_postinst() {
	elog "You must create /etc/tor/torrc, you can use the sample that is in that directory"
	elog "To have privoxy and tor working together you must add:"
	elog "forward-socks4a / localhost:9050 ."
	elog "(notice the . at the end of the line)"
	elog "to /etc/privoxy/config"
}
