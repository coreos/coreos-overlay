# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

GITHUB_URI="github.com/docker/containerd"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.7"

EGIT_COMMIT="4ab9917febca54791c5f071a9d1f404867857fcc" # v0.2.6
SRC_URI="https://${GITHUB_URI}/archive/v${PV}.tar.gz -> ${P}.tar.gz"
KEYWORDS="amd64 arm64"

inherit coreos-go systemd

DESCRIPTION="A daemon to control runC"
HOMEPAGE="https://containerd.tools"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="hardened +seccomp"

DEPEND=""
RDEPEND="~app-emulation/docker-runc-1.0.0_rc2
	seccomp? ( sys-libs/libseccomp )"

S=${WORKDIR}/${P}/src/${COREOS_GO_PACKAGE}

RESTRICT="test"

src_unpack() {
	mkdir -p "${S}"
	tar --strip-components=1 -C "${S}" -xf "${DISTDIR}/${A}"
}

src_compile() {
	local options=( $(usex seccomp "seccomp" '') )
	export GOPATH="${WORKDIR}/${P}" # ${PWD}/vendor
	LDFLAGS=$(usex hardened '-extldflags -fno-PIC' '') emake GIT_COMMIT="$EGIT_COMMIT" BUILDTAGS="${options[@]}"
}

src_install() {
	dobin bin/containerd* bin/ctr
	systemd_dounit "${FILESDIR}/containerd.service"
}
