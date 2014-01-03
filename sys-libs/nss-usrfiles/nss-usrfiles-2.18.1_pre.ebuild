# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
CROS_WORKON_COMMIT="47016ef8e5fb5436d62bd34fea69f15b9f3343c1"
CROS_WORKON_PROJECT="marineam/nss-altfiles"
CROS_WORKON_LOCALNAME="nss-altfiles"
CROS_WORKON_REPO="git://github.com"

inherit cros-workon

# The default files are provided by baselayout
BASELAYOUT_PV="2.2"
BASELAYOUT_P="baselayout-${BASELAYOUT_PV}"

DESCRIPTION="NSS module for data sources under /usr on for CoreOS"
HOMEPAGE="https://github.com/marineam/nss-altfiles"
SRC_URI="mirror://gentoo/${BASELAYOUT_P}.tar.bz2
	http://dev.gentoo.org/~vapier/dist/${BASELAYOUT_P}.tar.bz2"

LICENSE="LGPL-2.1+"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND=""
RDEPEND=""

src_unpack() {
	cros-workon_src_unpack
	default
}

src_configure() {
	: # Don't bother with the custom configure script.
}

src_compile() {
	emake DATADIR=/usr/share/nss MODULE_NAME=usrfiles
}

src_install() {
	dolib.so libnss_usrfiles.so.2

	insinto /usr/lib/tmpfiles.d
	newins "${FILESDIR}/tmpfiles.conf" "${PN}.conf"

	insinto /usr/share/nss
	doins "${FILESDIR}/nsswitch.conf"
	# imported from glibc 2.18 (not provided by baselayout)
	doins "${FILESDIR}/rpc"

	# gentoo defaults from baselayout
	for file in hosts networks protocols services; do
		doins "${WORKDIR}/${BASELAYOUT_P}/etc/${file}"
	done
}
