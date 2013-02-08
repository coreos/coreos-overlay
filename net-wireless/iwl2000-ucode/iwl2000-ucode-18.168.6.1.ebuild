# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

MY_PN="iwlwifi-2000-ucode"

DESCRIPTION="Intel (R) Centrino Wireless-N 2200 ucode"
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
	doins "${S}/iwlwifi-2000-6.ucode" || die

	dodoc README* || die "dodoc failed"
}
