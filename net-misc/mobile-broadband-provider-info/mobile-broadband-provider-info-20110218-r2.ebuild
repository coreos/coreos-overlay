# Copyright 2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/mobile-broadband-provider-info/mobile-broadband-provider-info-20100510.ebuild,v 1.4 2010/10/19 19:09:09 ranger Exp $

EAPI="2"

inherit eutils gnome.org

DESCRIPTION="Database of mobile broadband service providers"
HOMEPAGE="http://live.gnome.org/NetworkManager/MobileBroadband/ServiceProviders"

LICENSE="CC-PD"
SLOT="0"
KEYWORDS="amd64 ~arm ppc x86"
IUSE=""

RDEPEND=""
DEPEND=""

src_prepare() {
	# update to latest version of the database
        elog "Applying patch"
	epatch "${FILESDIR}"/mobile-broadband-provider-info-20110218-r1.patch
	epatch "${FILESDIR}"/0001-Add-gsm-APN-information-for-KTF-and-SKTelecom.patch
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
	dodoc NEWS README || die "dodoc failed"
}
