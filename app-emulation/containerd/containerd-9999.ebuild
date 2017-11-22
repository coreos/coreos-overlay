# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

GITHUB_URI="github.com/containerd/containerd"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.9"

if [[ ${PV} == *9999 ]]; then
	EGIT_REPO_URI="https://${GITHUB_URI}.git"
	inherit git-r3
else
	# Update the patch number when setting commit.
	# The patch number is arbitrarily chosen as the number of commits since the
	# tagged version.
	# e.g. git log --oneline v1.0.0-beta.2..${EGIT_COMMIT} | wc -l
	EGIT_COMMIT="992280e8e265f491f7a624ab82f3e238be086e49"
	SRC_URI="https://${GITHUB_URI}/archive/${EGIT_COMMIT}.tar.gz -> ${P}.tar.gz"
	KEYWORDS="amd64 arm64"
	inherit vcs-snapshot
	MAKE_VERSION_ARGS="REVISION=${EGIT_COMMIT} VERSION=v1.0.0-beta.2-53-g992280e8"
fi

inherit coreos-go systemd

DESCRIPTION="A daemon to control runC"
HOMEPAGE="https://containerd.tools"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="hardened +seccomp"

DEPEND=""
RDEPEND=">=app-emulation/docker-runc-1.0.0_rc4
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
	LDFLAGS=$(usex hardened '-extldflags -fno-PIC' '') emake ${MAKE_VERSION_ARGS} BUILDTAGS="${options[@]}"
}

src_install() {
	dobin bin/containerd* bin/ctr
	systemd_newunit "${FILESDIR}/${PN}-1.0.0.service" "${PN}.service"
	insinto /usr/share/containerd
	doins "${FILESDIR}/config.toml" 
}
