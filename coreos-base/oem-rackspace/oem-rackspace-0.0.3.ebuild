#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=5
inherit git-2

DESCRIPTION="oem suite for rackspace images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

EGIT_REPO_URI="https://github.com/coreos/nova-agent-container.git"
EGIT_COMMIT="44af5608e9a9e43fe83d05f03049a1f66d564930"

RDEPEND="
	coreos-base/nova-agent-watcher
"

src_prepare() {
	sed -e "s\\@@OEM_VERSION_ID@@\\${PVR}\\g" \
	    "${FILESDIR}/cloud-config.yml" > "${T}/cloud-config.yml" || die
}

src_install() {
	dodir /usr/share/oem/nova-agent
	rsync --exclude=".git" -aq "${S}/" "${D}/usr/share/oem/nova-agent/" || die
	insinto "/usr/share/oem"
	doins "${T}/cloud-config.yml"
	doins "${FILESDIR}/grub.cfg"

	into "/usr/share/oem"
	dobin "${FILESDIR}/coreos-setup-environment"
}
