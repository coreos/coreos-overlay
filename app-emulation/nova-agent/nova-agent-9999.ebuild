#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
EGIT_REPO_URI="https://github.com/coreos/openstack-guest-agents-unix"
inherit toolchain-funcs systemd git-2

DESCRIPTION="Rackspace Nova Agent"
HOMEPAGE="https://github.com/rackerlabs/openstack-guest-agents-unix"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

DEPEND="dev-util/patchelf"
RDEPEND="dev-python/pyxenstore"

src_configure() {
	./autogen.sh
	econf
}

src_compile() {
	emake
}

src_install() {
	emake DESTDIR="${D}" install
	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
}
