# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/mantle"
CROS_WORKON_LOCALNAME="mantle"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/quantum/mantle"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="ba5d6cd46eb6ec14a73f537902a1ac8f8776faad"
	KEYWORDS="amd64 arm64"
fi

inherit coreos-go cros-workon

DESCRIPTION="Mantle: Gluing CoreOS together"
HOMEPAGE="https://github.com/coreos/mantle"
LICENSE="Apache-2"
SLOT="0"

RDEPEND=">=net-dns/dnsmasq-2.72[dhcp,ipv6]"

src_compile() {
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

