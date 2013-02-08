# Copyright 1999-2010 Gentoo Foundation

EAPI="3"

DESCRIPTION="Zinnia learning data for Japanese"
HOMEPAGE="http://tegaki.org/"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${P}.zip"

LICENSE="LGPL"
SLOT="0"
KEYWORDS="amd64 x86 arm"

RDEPEND="app-i18n/zinnia"

src_install() {
	mkdir -p "${D}/usr/share/tegaki/models/zinnia" || die
	install handwriting-ja.meta handwriting-ja.model \
		"${D}/usr/share/tegaki/models/zinnia/" || die
}
