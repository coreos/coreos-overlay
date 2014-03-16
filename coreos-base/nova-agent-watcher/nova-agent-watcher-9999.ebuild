# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/nova-agent-watcher"
CROS_WORKON_LOCALNAME="nova-agent-watcher"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="18675c351e2c61a9f193c836a98473e8e6ecf994" 
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

DESCRIPTION="nova-agent-watcher"
HOMEPAGE="https://github.com/coreos/nova-agent-watcher"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"

src_compile() {
	./build || die
}

src_install() {
	dobin ${S}/scripts/gentoo-to-networkd
	dobin ${S}/bin/nova-agent-watcher
}
