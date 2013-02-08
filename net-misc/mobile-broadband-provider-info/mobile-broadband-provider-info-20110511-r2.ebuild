# Copyright 2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/mobile-broadband-provider-info/mobile-broadband-provider-info-20100510.ebuild,v 1.4 2010/10/19 19:09:09 ranger Exp $

EAPI="2"

inherit autotools eutils git

DESCRIPTION="Database of mobile broadband service providers"
HOMEPAGE="http://live.gnome.org/NetworkManager/MobileBroadband/ServiceProviders"
EGIT_REPO_URI="git://git.gnome.org/mobile-broadband-provider-info"
EGIT_COMMIT="d9995ef693cb1ea7237f928df18e03cccba96f16"

LICENSE="CC-PD"
SLOT="0"
KEYWORDS="amd64 arm ppc x86"
IUSE=""

RDEPEND=""
DEPEND=""

src_prepare() {
	# update to latest version of the database
	elog "Applying patches"
	epatch "${FILESDIR}"/0001-Add-gsm-APN-information-for-KTF-and-SKTelecom.patch
	epatch "${FILESDIR}"/0001-Remove-NetColgne-26203-it-interferes-with-eplus.patch
	epatch "${FILESDIR}"/0001-Vodafone-Update-with-data-from-mbb-apn-settings-2011.patch
}

src_compile() {
	eautoreconf || die "eautoreconf failed"
	econf || die "econf failed"
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
	dodoc NEWS README || die "dodoc failed"
}
