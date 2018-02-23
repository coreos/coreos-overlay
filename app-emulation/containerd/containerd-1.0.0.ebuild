# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

GITHUB_URI="github.com/containerd/containerd"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.9"

if [[ ${PV} == *9999 ]]; then
	EGIT_REPO_URI="https://${GITHUB_URI}.git"
	inherit git-r3
else
	MY_PV="${PV/_rc/-rc.}"
	EGIT_COMMIT="v${MY_PV}"
	CONTAINERD_COMMIT="89623f28b87a6004d4b785663257362d1658a729"
	SRC_URI="https://${GITHUB_URI}/archive/${EGIT_COMMIT}.tar.gz -> ${P}.tar.gz"
	KEYWORDS="amd64 arm64"
	inherit vcs-snapshot
	MAKE_VERSION_ARGS="REVISION=${CONTAINERD_COMMIT} VERSION=v${PV}"
fi

inherit coreos-go systemd

DESCRIPTION="A daemon to control runC"
HOMEPAGE="https://containerd.tools"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="hardened +seccomp"

DEPEND=""
RDEPEND="=app-emulation/docker-runc-1.0.0_rc4_p171
	seccomp? ( sys-libs/libseccomp )"

S=${WORKDIR}/${P}/src/${COREOS_GO_PACKAGE}

src_unpack() {
	mkdir -p "${S}"
	tar --strip-components=1 -C "${S}" -xf "${DISTDIR}/${A}"
}

src_compile() {
	local options=( $(usex seccomp "seccomp" "") )
	export GOPATH="${WORKDIR}/${P}" # ${PWD}/vendor
	LDFLAGS=$(usex hardened '-extldflags -fno-PIC' '') emake ${MAKE_VERSION_ARGS} BUILDTAGS="${options[@]}"
}

src_install() {
	dobin bin/containerd{-shim,-stress,} bin/ctr
	systemd_newunit "${FILESDIR}/${PN}-1.0.0.service" "${PN}.service"
	insinto /usr/share/containerd
	doins "${FILESDIR}/config.toml"
}
