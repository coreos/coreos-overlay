# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/mobile-broadband-provider-info/mobile-broadband-provider-info-20090602.ebuild,v 1.1 2009/08/24 13:21:52 dagger Exp $

DESCRIPTION="Database of mobile broadband service providers"
HOMEPAGE="http://live.gnome.org/NetworkManager/MobileBroadband/ServiceProviders"
SRC_URI="http://dev.gentoo.org/~dagger/files/${P}.tar.bz2"

LICENSE="CC-PD"
SLOT="0"
KEYWORDS="~arm ~amd64 ~x86"
IUSE=""
RDEPEND=""

DEPEND=""

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
}
