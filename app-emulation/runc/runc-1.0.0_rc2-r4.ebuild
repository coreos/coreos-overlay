# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

GITHUB_URI="github.com/opencontainers/runc"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.6"
# the commit of runc that docker uses.
# see https://github.com/docker/docker/blob/v1.13.0/hack/dockerfile/binaries-commits#L4
# Note: this commit is only really present in `docker/runc` in the 'docker/1.13.x' branch
COMMIT_ID="2f7393a47307a16f8cee44a37b262e8b81021e3e"

inherit eutils flag-o-matic coreos-go-depend vcs-snapshot

DESCRIPTION="runc container cli tools"
HOMEPAGE="http://runc.io"

SRC_URI="https://${GITHUB_URI}/archive/${COMMIT_ID}.tar.gz -> ${P}.tar.gz"
KEYWORDS="amd64 arm64"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="apparmor selinux +seccomp"

DEPEND=""
RDEPEND="
	apparmor? ( sys-libs/libapparmor )
	seccomp? ( sys-libs/libseccomp )
"

src_prepare() {
	epatch "${FILESDIR}/0001-Makefile-do-not-install-dependencies-of-target.patch"
	epatch "${FILESDIR}/0002-Dont-set-label-for-mqueue-under-userns.patch"

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
		$(usev selinux)
	)

	emake BUILDTAGS="${options[*]}"
}

src_install() {
	dobin runc
}
