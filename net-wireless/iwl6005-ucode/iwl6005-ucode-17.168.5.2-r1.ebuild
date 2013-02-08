# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-wireless/iwl6000-ucode/iwl6000-ucode-9.221.4.1.ebuild,v 1.2 2010/07/20 22:05:39 flameeyes Exp $

MY_PN="iwlwifi-6000g2a-ucode"

DESCRIPTION="Intel (R) Wireless WiFi Advanced N 6000 ucode"
HOMEPAGE="http://intellinuxwireless.org/?p=iwlwifi"
SRC_URI="http://intellinuxwireless.org/iwlwifi/downloads/${MY_PN}-${PV}.tgz"

LICENSE="ipw3945"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""
RDEPEND=""

DEPEND="|| ( >=sys-fs/udev-096 >=sys-apps/hotplug-20040923 )"

S="${WORKDIR}/${MY_PN}-${PV}"

src_compile() { :; }

src_install() {
	insinto /lib/firmware
	doins "${S}/iwlwifi-6000g2a-5.ucode" || die

	dodoc README* || die "dodoc failed"
}
