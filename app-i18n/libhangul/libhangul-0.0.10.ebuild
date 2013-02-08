# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-i18n/libhangul/libhangul-0.0.10.ebuild,v
# 1.1 2009/11/05 23:14:11 matsuu Exp $

DESCRIPTION="libhangul is a generalized and portable library for processing
hangul."
HOMEPAGE="http://kldp.net/projects/hangul/"
SRC_URI="mirror://gentoo/${P}.tar.gz"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="~alpha ~amd64 ~ppc ~x86"
IUSE=""

src_install() {
    emake DESTDIR="${D}" install || die "emake install failed"

    dodoc AUTHORS ChangeLog NEWS README
}
