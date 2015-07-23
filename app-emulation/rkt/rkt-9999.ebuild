# Copyright (c) 2015 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/rkt"
CROS_WORKON_LOCALNAME="rkt"
CROS_WORKON_REPO="git://github.com"
AUTOTOOLS_AUTORECONF=yes
AUTOTOOLS_IN_SOURCE_BUILD=yes
inherit autotools-utils cros-workon flag-o-matic systemd toolchain-funcs

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="9579f4bf57851a1a326c81ec2ab0ed2fdfab8d24" # v0.7.0
    KEYWORDS="amd64"
fi

DESCRIPTION="App Container runtime"
HOMEPAGE="https://github.com/coreos/rkt"

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.3
	app-arch/cpio
	sys-fs/squashfs-tools
	dev-perl/Capture-Tiny"
RDEPEND="!app-emulation/rocket"

src_configure() {
	local myeconfargs=(
		--with-stage1=host
	)

	# Go's 6l linker does not support PIE, disable so cgo binaries
	# which use 6l+gcc for linking can be built correctly.
	if gcc-specs-pie; then
		append-ldflags -nopie
	fi

	export CC=$(tc-getCC)
	export CGO_ENABLED=1
	export CGO_CFLAGS="${CFLAGS}"
	export CGO_CPPFLAGS="${CPPFLAGS}"
	export CGO_CXXFLAGS="${CXXFLAGS}"
	export CGO_LDFLAGS="${LDFLAGS}"

	autotools-utils_src_configure
}

src_install() {
	dobin "${S}/build-${P}/bin/rkt"

	insinto /usr/share/rkt
	dobin "${S}/build-${P}/bin/stage1.aci"

	systemd_dounit "${FILESDIR}"/${PN}-gc.service
	systemd_dounit "${FILESDIR}"/${PN}-gc.timer
	systemd_dounit "${S}"/dist/init/systemd/${PN}-metadata.service
	systemd_dounit "${S}"/dist/init/systemd/${PN}-metadata.socket

	systemd_enable_service sockets.target ${PN}-metadata.socket
}
