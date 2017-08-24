# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

GITHUB_URI="github.com/containerd/containerd"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.8"

if [[ ${PV} == *9999 ]]; then
	EGIT_REPO_URI="https://${GITHUB_URI}.git"
	inherit git-r3
else
	# Update the patch number when setting commit.
	# The patch number is arbitrarily chosen as the number of commits since the
	# tagged version.
	# e.g. git log --oneline v0.2.9..${EGIT_COMMIT} | wc -l
	EGIT_COMMIT="6e23458c129b551d5c9871e5174f6b1b7f6d1170"
	SRC_URI="https://${GITHUB_URI}/archive/${EGIT_COMMIT}.tar.gz -> ${P}.tar.gz"
	KEYWORDS="amd64 arm64"
	inherit vcs-snapshot
fi

inherit coreos-go systemd

DESCRIPTION="A daemon to control runC"
HOMEPAGE="https://containerd.tools"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="hardened +seccomp"

DEPEND=""
RDEPEND=">=app-emulation/docker-runc-1.0.0_rc3
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
