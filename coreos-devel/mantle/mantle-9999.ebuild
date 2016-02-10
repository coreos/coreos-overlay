# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="quantum/coreos-mantle"
CROS_WORKON_LOCALNAME="mantle"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/mantle"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="c4109265954875e89d755b8e62fa3775824062f1" # v0.0.9
	KEYWORDS="amd64 arm64"
fi

inherit coreos-go cros-workon

DESCRIPTION="Mantle: Gluing CoreOS together"
HOMEPAGE="https://github.com/coreos/mantle"
LICENSE="Apache-2"
SLOT="0"

RDEPEND=">=net-dns/dnsmasq-2.72[dhcp,ipv6]"

src_compile() {
	if [[ "${PV}" == 9999 ]]; then
		GO_LDFLAGS="-X ${COREOS_GO_PACKAGE}/version.Version $(get_semver)"
	else
		GO_LDFLAGS="-X ${COREOS_GO_PACKAGE}/version.Version ${PV}"
	fi

	for cmd in cork kola kolet ore plume; do
		go_build "${COREOS_GO_PACKAGE}/cmd/${cmd}"
	done
}

src_install() {
	for cmd in cork kola ore plume; do
		dobin "${GOBIN}"/"${cmd}"
	done

	exeinto /usr/lib/kola/"$(go_get_arch)"
	doexe "${GOBIN}"/kolet
}

