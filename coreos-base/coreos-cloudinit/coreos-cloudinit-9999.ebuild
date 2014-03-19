# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/coreos-cloudinit"
CROS_WORKON_LOCALNAME="coreos-cloudinit"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="c2faaa503b3dfbaa58960db36bdc77a9c7b02ce2"  # v0.1.2+git
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

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
	systemd_dounit "${FILESDIR}"/coreos-cloudinit-oem.service
	systemd_dounit "${FILESDIR}"/cloud-config.target
	systemd_enable_service default.target coreos-cloudinit-oem.service
	systemd_enable_service default.target cloud-config.target
}
