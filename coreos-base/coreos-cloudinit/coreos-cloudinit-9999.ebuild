# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/coreos-cloudinit"
CROS_WORKON_LOCALNAME="coreos-cloudinit"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="9ba3b18b59bb23888e37ed2c9ebb3d0252385073" # tag v0.8.0
	KEYWORDS="amd64"
fi

inherit cros-workon systemd udev

DESCRIPTION="coreos-cloudinit"
HOMEPAGE="https://github.com/coreos/coreos-cloudinit"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2
	!<coreos-base/coreos-init-0.0.1-r69"

RDEPEND="
	>=sys-apps/shadow-4.1.5.1
"

src_compile() {
	./build || die
}

src_install() {
	dobin ${S}/bin/coreos-cloudinit
	udev_dorules units/*.rules
	systemd_dounit units/*.mount
	systemd_dounit units/*.path
	systemd_dounit units/*.service
	systemd_dounit units/*.target
	systemd_enable_service default.target system-config.target
	systemd_enable_service default.target user-config.target
}
