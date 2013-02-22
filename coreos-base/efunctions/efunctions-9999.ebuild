#
# Copyright (c) 2013 Brandon Philips. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
CROS_WORKON_PROJECT="coreos/efunctions"
CROS_WORKON_LOCALNAME="efunctions"
inherit toolchain-funcs cros-workon

DESCRIPTION="standalone replacement for functions.sh"
HOMEPAGE="https://bitbucket.org/coreos/efunctions"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

src_install() {
	into "/usr/lib/${PN}"
	dodir "${S}/efunctions"
	doins "functions.sh"
	dosym /usr/lib/${PN}/functions.sh /etc/init.d/functions.sh
}
