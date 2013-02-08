# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
inherit eutils flag-o-matic

DESCRIPTION="English IME for testing"
HOMEPAGE="http://github.com/yusukes/ibus-zinnia"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/ibus-zinnia-0.0.3.tar.gz"
#RESTRICT="mirror"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 arm x86"

RDEPEND=">=app-i18n/ibus-1.3.99"
DEPEND="${RDEPEND}
        dev-util/pkgconfig
        >=sys-devel/gettext-0.16.1"

# Tarballs in github.com use a special directory naming rule:
#   "<github_user_name>-<project_name>-<last_commit_in_the_tarball>"
SRC_DIR="yusukes-ibus-zinnia-910d66d"

src_prepare() {
    cd "${SRC_DIR}" || die

    epatch "${FILESDIR}"/english-m.patch || die
}

src_configure() {
    cd "${SRC_DIR}" || die

    append-cflags -Wall -Werror
    NOCONFIGURE=1 ./autogen.sh || die
    econf || die
}

src_install() {
    cd "${SRC_DIR}" || die

    emake DESTDIR="${T}" install || die

    exeinto /usr/libexec || die
    newexe "${T}"/usr/libexec/ibus-engine-zinnia ibus-engine-english-m || die

    insinto /usr/share/ibus/component || die
    newins "${T}"/usr/share/ibus/component/zinnia.xml english-m.xml || die
}
