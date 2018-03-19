# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=5

GITHUB_URI="github.com/docker/${PN}"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.7"

MY_PV="${PV/_/-}"
EGIT_COMMIT="v${MY_PV}"
SRC_URI="https://${GITHUB_URI}/archive/${EGIT_COMMIT}.tar.gz -> ${P}.tar.gz"
KEYWORDS="amd64 arm64"
inherit vcs-snapshot

inherit coreos-go systemd

DESCRIPTION="A daemon to control runC"
HOMEPAGE="https://containerd.tools"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="seccomp"

DEPEND=""
RDEPEND="app-emulation/runc
	seccomp? ( sys-libs/libseccomp )"

src_compile() {
	local options=( $(usev seccomp) )
	LDFLAGS= emake GIT_COMMIT="$EGIT_COMMIT" BUILDTAGS="${options[*]}"
}

src_install() {
	dobin bin/containerd* bin/ctr

	systemd_dounit "${FILESDIR}/containerd.service"
}
