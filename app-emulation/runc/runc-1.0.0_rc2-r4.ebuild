# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

GITHUB_URI="github.com/opencontainers/runc"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.6"
COMMIT_ID="c91b5bea4830a57eac7882d7455d59518cdf70ec" # v1.0.0-rc2

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
	epatch "${FILESDIR}/runc-1.0.0_rc2-init-non-dumpable.patch"
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
