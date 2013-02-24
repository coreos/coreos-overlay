#
# Copyright (c) 2013 Brandon Philips. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=3
CROS_WORKON_PROJECT="coreos/efunctions"
CROS_WORKON_LOCALNAME="efunctions"
inherit eutils cros-workon

DESCRIPTION="standalone replacement for functions.sh"
HOMEPAGE="https://bitbucket.org/coreos/efunctions"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

src_install() {
	emake DESTDIR="${D}" install || die

	# make functions.sh available in /etc/init.d
	# Note: we cannot replace the symlink with a file here, or Portage will
	# config-protect it, and etc-update can't handle symlink to file updates
	dodir etc/init.d
	dosym ../../usr/lib/efunctions/functions.sh /etc/init.d/functions.sh

    local dst_dir=/usr/lib/${PN}
	dodir $dst_dir || die
	insinto $dst_dir

	doins ${S}/functions.sh
	doins -r ${S}/efunctions

	fperms -R +x $dst_dir

}
