# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/coreos-cloudinit"
CROS_WORKON_LOCALNAME="coreos-cloudinit"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="e4965ec33d06f0c71622aad59d5f59f9aaca7c00"  # v0.1.2
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

DESCRIPTION="coreos-cloudinit"
HOMEPAGE="https://github.com/coreos/coreos-cloudinit"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"

RDEPEND="
	>=sys-apps/shadow-4.1.5.1
"

src_compile() {
	./build || die
}

src_install() {
	dobin ${S}/bin/coreos-cloudinit
}
