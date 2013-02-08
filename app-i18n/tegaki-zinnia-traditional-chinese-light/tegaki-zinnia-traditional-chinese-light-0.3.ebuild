# Copyright 1999-2011 Gentoo Foundation

EAPI="3"

DESCRIPTION="Zinnia learning data for traditional Chinese"
HOMEPAGE="http://tegaki.org/"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${P}.zip"

LICENSE="LGPL"
SLOT="0"
KEYWORDS="amd64 x86 arm"

RDEPEND="app-i18n/zinnia"

src_install() {
	mkdir -p "${D}/usr/share/tegaki/models/zinnia" || die
	install handwriting-zh_TW.meta handwriting-zh_TW.model \
		"${D}/usr/share/tegaki/models/zinnia/" || die
}
