# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

GITHUB_URI="github.com/opencontainers/runc"
COREOS_GO_PACKAGE="${GITHUB_URI}"
COREOS_GO_VERSION="go1.8"
# the commit of runc that docker uses.
# see https://github.com/docker/docker-ce/blob/v17.06.2-ce/components/engine/hack/dockerfile/binaries-commits#L6
# Note: this commit is only really present in `docker/runc` in the 'docker/17.06' branch
# Update the patch number when this commit is changed (i.e. the _p in the ebuild).
# The patch version is arbitrarily the number of commits since the tag version
# spcified in the ebuild name. For example:
# $ git log --oneline v1.0.0-rc3..${COMMIT_ID} | wc -l
COMMIT_ID="810190ceaa507aa2727d7ae6f4790c76ec150bd2"

inherit eutils flag-o-matic coreos-go vcs-snapshot

SRC_URI="https://${GITHUB_URI}/archive/${COMMIT_ID}.tar.gz -> ${P}.tar.gz"
KEYWORDS="amd64 arm64"

DESCRIPTION="runc container cli tools (docker fork)"
HOMEPAGE="http://runc.io"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="apparmor ambient hardened +seccomp selinux"

RDEPEND="
	apparmor? ( sys-libs/libapparmor )
	seccomp? ( sys-libs/libseccomp )
	!app-emulation/runc
"

S=${WORKDIR}/${P}/src/${COREOS_GO_PACKAGE}

RESTRICT="test"

src_unpack() {
	mkdir -p "${S}"
	tar --strip-components=1 -C "${S}" -xf "${DISTDIR}/${A}"
}

PATCHES=(
	"${FILESDIR}/0001-Delay-unshare-of-clone-newipc-for-selinux.patch"
	"${FILESDIR}/0002-libcontainer-default-mount-propagation-correctly.patch"
)

src_compile() {
	# Taken from app-emulation/docker-1.7.0-r1
	export CGO_CFLAGS="-I${ROOT}/usr/include"
	export CGO_LDFLAGS="$(usex hardened '-fno-PIC ' '')
		-L${ROOT}/usr/$(get_libdir)"

	# build up optional flags
	local options=(
		$(usex apparmor 'apparmor' '')
		$(usex ambient 'ambient' '')
		$(usex seccomp 'seccomp' '')
		$(usex selinux 'selinux' '')
	)

	# CoreOS: Don't try to install dependencies.
	sed -i 's/go build -i /go build /' Makefile

	GOPATH="${WORKDIR}/${P}" emake BUILDTAGS="${options[*]}" \
		COMMIT="${COMMIT_ID}"
}

src_install() {
	dobin runc
}
