# Copyright 1999-2019 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=6
EGO_PN="github.com/opencontainers/${PN}"

COREOS_GO_PACKAGE="${EGO_PN}"
COREOS_GO_VERSION="go1.10"
# the commit of runc that docker uses.
# see https://github.com/docker/docker-ce/blob/v19.03.1/components/engine/hack/dockerfile/install/runc.installer#L7
# Update the patch number when this commit is changed (i.e. the _p in the ebuild).
# The patch version is arbitrarily the number of commits since the tag version
# specified in the ebuild name. For example:
# $ git log --oneline v1.0.0-rc8..${RUNC_COMMIT} | wc -l
RUNC_COMMIT="425e105d5a03fabd737a126ad93d62a9eeede87f"

inherit eutils flag-o-matic coreos-go vcs-snapshot

SRC_URI="https://${EGO_PN}/archive/${RUNC_COMMIT}.tar.gz -> ${P}.tar.gz"
KEYWORDS="amd64 arm64"

DESCRIPTION="runc container cli tools"
HOMEPAGE="http://runc.io"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="ambient apparmor hardened +kmem +seccomp selinux"

RDEPEND="
	apparmor? ( sys-libs/libapparmor )
	seccomp? ( sys-libs/libseccomp )
	!app-emulation/docker-runc
"

S=${WORKDIR}/${P}/src/${EGO_PN}

src_unpack() {
	mkdir -p "${S}"
	tar --strip-components=1 -C "${S}" -xf "${DISTDIR}/${A}"
}

src_prepare() {
	default
	sed -i -e "/^GIT_BRANCH/d"\
		-e "/^GIT_BRANCH_CLEAN/d"\
		-e "/^COMMIT_NO/d"\
		-e "s/COMMIT :=.*/COMMIT := ${RUNC_COMMIT}/"\
		Makefile || die
}

PATCHES=(
	"${FILESDIR}/0001-Delay-unshare-of-clone-newipc-for-selinux.patch"
)

src_compile() {
	# Taken from app-emulation/docker-1.7.0-r1
	export CGO_CFLAGS="-I${ROOT}/usr/include"
	export CGO_LDFLAGS="$(usex hardened '-fno-PIC ' '')
		-L${ROOT}/usr/$(get_libdir)"

	# build up optional flags
	local options=(
		$(usex ambient 'ambient' '')
		$(usex apparmor 'apparmor' '')
		$(usex seccomp 'seccomp' '')
		$(usex selinux 'selinux' '')
		$(usex kmem '' 'nokmem')
	)

	GOPATH="${WORKDIR}/${P}" emake BUILDTAGS="${options[*]}"
}

src_install() {
	dobin runc
	dodoc README.md PRINCIPLES.md
}
