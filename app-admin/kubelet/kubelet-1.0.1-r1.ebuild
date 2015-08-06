#
# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=5

inherit flag-o-matic systemd toolchain-funcs

DESCRIPTION="Kubernetes Container Manager"
HOMEPAGE="http://kubernetes.io/"
KEYWORDS="amd64"
SRC_URI="https://github.com/GoogleCloudPlatform/kubernetes/archive/v${PV}.tar.gz -> ${P}.tar.gz"
S="${WORKDIR}/kubernetes-${PV}"

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND="dev-lang/go"
RDEPEND="net-misc/socat"

src_prepare() {
	epatch "${FILESDIR}/0001-Plumb-linker-flags-through-from-the-Makefile.patch"

	if gcc-specs-pie; then
		append-ldflags -nopie
	fi

	export CC=$(tc-getCC)
	export CGO_ENABLED=${CGO_ENABLED:-1}
	export CGO_CFLAGS="${CFLAGS}"
	export CGO_CPPFLAGS="${CPPFLAGS}"
	export CGO_CXXFLAGS="${CXXFLAGS}"
	export CGO_LDFLAGS="${LDFLAGS}"
}

src_compile() {
	emake all WHAT="cmd/${PN}" GOFLAGS="-x" GOLDFLAGS="-extldflags '${LDFLAGS}'"
}

src_install() {
	dobin "${S}/_output/local/bin/linux/${ARCH}/${PN}"

	systemd_dounit "${FILESDIR}/kubelet.service"
}
