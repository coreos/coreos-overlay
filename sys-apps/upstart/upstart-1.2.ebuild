# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit autotools eutils

DESCRIPTION="Upstart is an event-based replacement for the init daemon"
HOMEPAGE="http://upstart.ubuntu.com/"
SRC_URI="http://upstart.at/download/1.x/${P}.tar.gz"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE="examples nls upstartdebug"

DEPEND=">=dev-libs/expat-2.0.0
	>=sys-apps/dbus-1.2.16
	nls? ( sys-devel/gettext )
	>=sys-libs/libnih-1.0.2"

RDEPEND=">=sys-apps/dbus-1.2.16
	>=sys-libs/libnih-1.0.2"


src_unpack() {
	unpack ${A}
	cd "${S}"

	# 1.3+ has scary user and chroot session support that we just
	# don't want to adopt yet, so we're sticking with 1.2 for the
	# near future. Backport some bug fixes from lp:upstart

	# -r 1326 - fix bug when /dev/console cannot be opened
	# chromium-os:18739
	epatch "${FILESDIR}"/upstart-1.2-silent-console.patch
	# -r 1280,1308,1309,1320,1329 - fix shell fd leak (and fix the fix)
	epatch "${FILESDIR}"/upstart-1.2-fix-shell-redirect.patch
	# -r 1281,1325,1327,1328 - update to use /proc/oom_score
	epatch "${FILESDIR}"/upstart-1.2-oom-score.patch
	# -r 1282 - add "kill signal" stanza (may be useful for us)
	epatch "${FILESDIR}"/upstart-1.2-kill-signal.patch

	# chromium-os:16450, prevent OOM killer by default
	epatch "${FILESDIR}"/upstart-1.2-default-oom_score_adj.patch

	# chromium-os:33165, make EXIT_STATUS!=* possible
	epatch "${FILESDIR}"/upstart-1.2-negate-match.patch

	# Patch to use kmsg at higher verbosity for logging; this is
	# our own patch because we can't just add --verbose to the
	# kernel command-line when we need to.
	if use upstartdebug; then
		epatch "${FILESDIR}"/upstart-1.2-log-verbosity.patch
	fi
}

src_compile() {
	econf --prefix=/ --includedir='${prefix}/usr/include' \
		$(use_enable nls) || die "econf failed"

	emake NIH_DBUS_TOOL=$(which nih-dbus-tool) \
		|| die "emake failed"
}

src_install() {
	emake DESTDIR="${D}" install || die "make install failed"

	if ! use examples ; then
		elog "Removing example .conf files."
		rm "${D}"/etc/init/*.conf
	fi
}
