# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

MY_PN="WALinuxAgent"
MY_PV="WALinuxAgent-${PV}"
MY_P="${MY_PN}-${MY_PV}"
S="${WORKDIR}/${MY_P}"

DESCRIPTION="Windows Azure Linux Agent"
HOMEPAGE="https://github.com/Azure/WALinuxAgent"
KEYWORDS="amd64"
SRC_URI="${HOMEPAGE}/archive/${MY_PV}.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND="dev-lang/python-oem"

src_install() {
	into "/usr/share/oem"
	dobin "${S}"/waagent

	insinto "/usr/share/oem"
	doins "${FILESDIR}"/waagent.conf
}
