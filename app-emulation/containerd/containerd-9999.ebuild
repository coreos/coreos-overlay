# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=5

GITHUB_URI="github.com/docker/${PN}"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.7"

if [[ ${PV} == *9999 ]]; then
	EGIT_REPO_URI="https://${GITHUB_URI}.git"
	inherit git-r3
else
	# Update the patch number when setting commit.
	# The patch number is arbitrarily chosen as the number of commits since the
	# tagged version.
	# e.g.  git log ${base_version}..${EGIT_COMMIT} --oneline | wc -l
	# Note: 0.2.3 in the docker-1.13.x branch is not tagged, use 973f21f
	EGIT_COMMIT="aa8187dbd3b7ad67d8e5e3a15115d3eef43a7ed1"
	SRC_URI="https://${GITHUB_URI}/archive/${EGIT_COMMIT}.tar.gz -> ${P}.tar.gz"
	KEYWORDS="amd64 arm64"
	inherit vcs-snapshot
fi

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
