# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

DESCRIPTION="Windows Azure Linux Agent"
HOMEPAGE="https://github.com/Azure/WALinuxAgent"
KEYWORDS="amd64"
SRC_URI="${HOMEPAGE}/archive/v${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND="dev-lang/python-oem"

S="${WORKDIR}/WALinuxAgent-${PV}"

src_install() {
	into "/usr/share/oem"
	dobin "${S}/bin/waagent"

	insinto "/usr/share/oem/python/$(get_libdir)/python2.7/site-packages"
	doins -r "${S}/azurelinuxagent/"

	insinto "/usr/share/oem"
	doins "${FILESDIR}/waagent.conf"
}
