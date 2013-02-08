# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/openvpn/openvpn-2.1.4.ebuild,v 1.8 2011/03/21 08:22:40 xarthisius Exp $

EAPI=2

inherit eutils multilib toolchain-funcs autotools flag-o-matic

DESCRIPTION="OpenVPN is a robust and highly flexible tunneling application compatible with many OSes."
SRC_URI="http://swupdate.openvpn.org/as/as-openvpn-core/build/${P}.tar.gz"
HOMEPAGE="http://openvpn.net/"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ~mips ppc ppc64 s390 sh sparc x86 ~sparc-fbsd ~x86-fbsd"
IUSE="eurephia examples iproute2 ipv6 minimal pam passwordsave selinux ssl static pkcs11 userland_BSD"

DEPEND=">=dev-libs/lzo-1.07
	kernel_linux? (
		iproute2? ( sys-apps/iproute2[-minimal] ) !iproute2? ( sys-apps/net-tools )
	)
	!minimal? ( pam? ( virtual/pam ) )
	selinux? ( sec-policy/selinux-openvpn )
	ssl? ( >=dev-libs/openssl-0.9.6 )
	pkcs11? ( >=dev-libs/pkcs11-helper-1.05 )"
RDEPEND="${DEPEND}"

src_prepare() {
	sed -i \
		-e "s/gcc \${CC_FLAGS}/\${CC} \${CFLAGS} -Wall/" \
		-e "s/-shared/-shared \${LDFLAGS}/" \
		plugin/*/Makefile || die "sed failed"
	epatch "${FILESDIR}"/pkcs11-slot.patch
	epatch "${FILESDIR}"/iv_plat.patch
}

src_configure() {
	# basic.h defines a type 'bool' that conflicts with the altivec
	# keyword bool which has to be fixed upstream, see bugs #293840
	# and #297854.
	# For now, filter out -maltivec on ppc and append -mno-altivec, as
	# -maltivec is enabled implicitly by -mcpu and similar flags.
	(use ppc || use ppc64) && filter-flags -maltivec && append-flags -mno-altivec

	econf \
		$(use_enable passwordsave password-save) \
		$(use_enable pkcs11) \
		$(use_enable ssl) \
		$(use_enable ssl crypto) \
		$(use_enable iproute2)
}

src_compile() {

	if use static ; then
		sed -i -e '/^LIBS/s/LIBS = /LIBS = -static /' Makefile || die "sed failed"
	fi

	emake || die "make failed"
}

src_install() {
	emake DESTDIR="${D}" install || die "make install failed"
}

pkg_postinst() {
	# Add openvpn user so openvpn servers can drop privs
	# Clients should run as root so they can change ip addresses,
	# dns information and other such things.
	enewgroup openvpn
	enewuser openvpn "" "" "" openvpn
}
