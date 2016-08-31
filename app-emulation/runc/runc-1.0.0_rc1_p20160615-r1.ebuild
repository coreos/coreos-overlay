# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

GITHUB_URI="github.com/opencontainers/runc"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.6"
# the commit of runc that docker uses.
# see https://github.com/docker/docker/blob/v1.12.0/Dockerfile#L236
COMMIT_ID="cc29e3dded8e27ba8f65738f40d251c885030a28"

inherit eutils flag-o-matic coreos-go-depend vcs-snapshot

DESCRIPTION="runc container cli tools"
HOMEPAGE="http://runc.io"

SRC_URI="https://${GITHUB_URI}/archive/${COMMIT_ID}.tar.gz -> ${P}.tar.gz"
KEYWORDS="amd64 arm64"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="apparmor +seccomp"

DEPEND=""
RDEPEND="
	apparmor? ( sys-libs/libapparmor )
	seccomp? ( sys-libs/libseccomp )
"

src_prepare() {
	epatch "${FILESDIR}/0001-Makefile-do-not-install-dependencies-of-target.patch"
	# Work around https://github.com/golang/go/issues/14669
	# Remove after updating to go1.7
	filter-flags -O*

	go_export
}

src_compile() {
	# build up optional flags
	local options=(
		$(usev apparmor)
		$(usev seccomp)
	)

	emake BUILDTAGS="${options[*]}"
}

src_install() {
	dobin runc
}
