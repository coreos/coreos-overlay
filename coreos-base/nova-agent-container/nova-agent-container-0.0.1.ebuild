# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="nova agent for rackspace images"
HOMEPAGE="https://github.com/coreos/nova-agent-container"
SRC_URI="${HOMEPAGE}/archive/v${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

src_install() {
	dodir /usr/share/oem/nova-agent
	rsync -aq "${S}/" "${D}/usr/share/oem/nova-agent/" || die
}
