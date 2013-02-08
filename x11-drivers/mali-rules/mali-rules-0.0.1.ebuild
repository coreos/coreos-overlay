# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="Rules for setting permissions right on /dev/mali0"

LICENSE="BSD"
SLOT="0"
KEYWORDS="arm"
IUSE=""

# Because this ebuild has no source package, "${S}" doesn't get
# automatically created.  The compile phase depends on "${S}" to
# exist, so we make sure "${S}" refers to a real directory.
#
# The problem is apparently an undocumented feature of EAPI 4;
# earlier versions of EAPI don't require this.
S="${WORKDIR}"

src_install() {
	insinto /etc/udev/rules.d
	doins "${FILESDIR}"/50-mali.rules
}
