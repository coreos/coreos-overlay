# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/coreos-cloudinit"
CROS_WORKON_LOCALNAME="coreos-cloudinit"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/coreos-cloudinit"
inherit coreos-doc cros-workon systemd toolchain-funcs udev coreos-go

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
elif [[ "${PV}" == "1.5.1" ]]; then
	CROS_WORKON_COMMIT="fac805dc11951d32fe68a77200988828fa1ce48d" # tag v1.5.1
	KEYWORDS="amd64 arm64"
else
	CROS_WORKON_COMMIT="f816819c6dd562196a9e416829561ba849879f58" # tag v1.6.0
	KEYWORDS="amd64 ~arm64"
fi

DESCRIPTION="coreos-cloudinit"
HOMEPAGE="https://github.com/coreos/coreos-cloudinit"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND="!<coreos-base/coreos-init-0.0.1-r69"
RDEPEND="
	>=sys-apps/shadow-4.1.5.1
"

src_prepare() {
	coreos-go_src_prepare

	GOPATH+=":${S}/third_party"

	if gcc-specs-pie; then
		CGO_LDFLAGS+=" -fno-PIC"
	fi
}

src_compile() {
	GO_LDFLAGS="-X main.version $(git describe --dirty)" || die
	coreos-go_src_compile
}

src_install() {
	dobin ${GOBIN}/coreos-cloudinit
	udev_dorules units/*.rules
	systemd_dounit units/*.mount
	systemd_dounit units/*.path
	systemd_dounit units/*.service
	systemd_dounit units/*.target
	systemd_enable_service default.target system-config.target
	systemd_enable_service default.target user-config.target

	coreos-dodoc -r Documentation/*
}
