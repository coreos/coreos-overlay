#
# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=4

EGIT_REPO_URI="git://github.com/Azure/WALinuxAgent"
EGIT_COMMIT="b3f2619a854455675ae5f2ee14726659e0398af7" # WALinuxAgent-2.0.11
EGIT_MASTER="2.0"

inherit toolchain-funcs git-2

DESCRIPTION="Windows Azure Linux Agent"
HOMEPAGE="https://github.com/Azure/WALinuxAgent"
KEYWORDS="amd64"
SRC_URI=""

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
