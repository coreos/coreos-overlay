# Copyright 1999-2009 Gentoo Foundation
# Copyright 2010 Google, Inc.
# Distributed under the terms of the GNU General Public License v2
# $Header$

EAPI="2"
CROS_WORKON_COMMIT="e3afe51c44ea5b1ebbc6fc6561c46fd0608cde2a"
CROS_WORKON_TREE="c6b3d8822b63163c977a8d4e0bc9ef3c9cd7b011"
CROS_WORKON_PROJECT="chromiumos/third_party/trousers"

inherit autotools base cros-workon eutils linux-info

DESCRIPTION="An open-source TCG Software Stack (TSS) v1.1 implementation"
HOMEPAGE="http://trousers.sf.net"
LICENSE="CPL-1.0"
KEYWORDS="amd64 arm x86"
SLOT="0"
IUSE="doc tss_trace"

RDEPEND=">=dev-libs/openssl-0.9.7"

DEPEND="${RDEPEND}
	dev-util/pkgconfig"

## TODO: Check if this patch is useful for us.
## PATCHES=(	"${FILESDIR}/${PN}-0.2.3-nouseradd.patch" )

pkg_setup() {
	# New user/group for the daemon
	enewgroup tss
	enewuser tss -1 -1 /var/lib/tpm tss
}

src_prepare() {
	base_src_prepare

	sed -e "s/-Werror //" -i configure.in
	if use tss_trace ; then
		# Enable tracing of TSS calls.
		export CFLAGS="$CFLAGS -DTSS_TRACE"
	fi
	eautoreconf
}

src_compile() {
	tc-export CC CXX AR RANLIB LD NM
	export CCFLAGS="$CFLAGS"
	emake
}

src_install() {
	keepdir /var/lib/tpm
	emake DESTDIR="${D}" install || die
	dodoc AUTHORS ChangeLog NICETOHAVES README TODO
	use doc && dodoc doc/*
	newinitd "${FILESDIR}/tcsd.initd" tcsd
	newconfd "${FILESDIR}/tcsd.confd" tcsd

	# Install the empty system.data files
	dodir /etc/trousers
	insinto /etc/trousers
	doins "${S}"/dist/system.data.*
}

pkg_postinst() {
	elog "If you have problems starting tcsd, please check permissions and"
	elog "ownership on /dev/tpm* and ~tss/system.data"
}
