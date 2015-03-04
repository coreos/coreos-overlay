#
# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=4
CROS_WORKON_PROJECT="coreos/etcd"
CROS_WORKON_LOCALNAME="etcd"
CROS_WORKON_REPO="git://github.com"
inherit coreos-doc toolchain-funcs cros-workon systemd

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="d6523fe4638100c72f40cb282cd1232db13f7336" # v0.4.7
    KEYWORDS="amd64"
fi

DESCRIPTION="etcd"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="1"
IUSE="etcd_protocols_1 etcd_protocols_2"

# Sanity check that this version is indeed wanted!
REQUIRED_USE="etcd_protocols_${SLOT}"

DEPEND=">=dev-lang/go-1.2"
RDEPEND="!dev-db/etcd:0
	!etcd_protocols_2? ( !dev-db/etcd:2 )"

src_compile() {
	./build
}

src_install() {
	local libexec="libexec/${PN}/internal_versions"

	exeinto "/usr/${libexec}"
	newexe "${S}/bin/${PN}" ${SLOT}

	# protocol1 is default if protocol2 is disabled
	if ! use etcd_protocols_2; then
		dosym "../${libexec}/${SLOT}" /usr/bin/${PN}

		systemd_dounit "${FILESDIR}"/${PN}.service
		systemd_dotmpfilesd "${FILESDIR}"/${PN}.conf

		coreos-dodoc -r Documentation/*
	fi
}
