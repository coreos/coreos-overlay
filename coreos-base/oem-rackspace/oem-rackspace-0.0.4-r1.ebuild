#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=5

DESCRIPTION="oem suite for rackspace images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

RDEPEND="
	coreos-base/nova-agent-container
	coreos-base/nova-agent-watcher
"

S="${WORKDIR}"

src_prepare() {
	sed -e "s\\@@OEM_VERSION_ID@@\\${PVR}\\g" \
	    "${FILESDIR}/cloud-config.yml" > "${T}/cloud-config.yml" || die
}

src_install() {
	insinto "/usr/share/oem"
	doins "${T}/cloud-config.yml"
	doins "${FILESDIR}/grub.cfg"

	into "/usr/share/oem"
	dobin "${FILESDIR}/coreos-setup-environment"
}
