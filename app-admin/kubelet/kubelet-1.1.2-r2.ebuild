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
	epatch "${FILESDIR}/0001-kubelet-report-NodeReady-last-in-status-list.patch"
	epatch "${FILESDIR}/0002-explicitly-check-Ready-condition-in-validate-cluster.patch"
	epatch "${FILESDIR}/0003-kubelet-check-node-condition-by-type-rather-than-by-.patch"
	epatch "${FILESDIR}/0004-pkg-kubelet-force-NodeReady-condition-to-be-last-on-.patch"
	epatch "${FILESDIR}/0005-Bump-go-dockerclient-for-v1.10.x-support.patch"

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
