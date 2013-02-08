# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
inherit eutils flag-o-matic

DESCRIPTION="Zinnia hand-writing engine"
HOMEPAGE="http://github.com/yusukes/ibus-zinnia"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${P}.tar.gz"
#RESTRICT="mirror"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 arm x86"

RDEPEND=">=app-i18n/ibus-1.3.99
	 app-i18n/zinnia
	 app-i18n/tegaki-zinnia-japanese-light
	 app-i18n/tegaki-zinnia-simplified-chinese-light
	 app-i18n/tegaki-zinnia-traditional-chinese-light"
DEPEND="${RDEPEND}
	dev-util/pkgconfig
	>=sys-devel/gettext-0.16.1"

# Tarballs in github.com use a special directory naming rule:
#   "<github_user_name>-<project_name>-<last_commit_in_the_tarball>"
SRC_DIR="yusukes-ibus-zinnia-910d66d"

src_configure() {
    cd "${SRC_DIR}" || die

    append-cflags -Wall -Werror
    NOCONFIGURE=1 ./autogen.sh || die
    econf || die
}

src_install() {
    cd "${SRC_DIR}" || die

    emake DESTDIR="${D}" install || die
    dodoc AUTHORS ChangeLog NEWS README
}
