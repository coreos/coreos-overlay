# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-i18n/ibus-m17n/ibus-m17n-1.2.0.20090617.ebuild,v 1.1 2009/06/18 15:40:00 matsuu Exp $

EAPI="2"
inherit eutils

DESCRIPTION="The M17N engine IMEngine for IBus Framework"
HOMEPAGE="http://code.google.com/p/ibus/"
SRC_URI="http://ibus.googlecode.com/files/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="nls"

RDEPEND=">=app-i18n/ibus-1.3.99
	>=dev-libs/m17n-lib-1.6.1
	>=dev-db/m17n-db-1.6.1
	>=dev-db/m17n-contrib-1.1.10
	nls? ( virtual/libintl )"
DEPEND="${RDEPEND}
	>=chromeos-base/chromeos-chrome-16
	chromeos-base/chromeos-assets
	dev-libs/libxml2
	dev-util/pkgconfig
	>=sys-devel/gettext-0.16.1"

src_prepare() {
	# Make it possible to override the DEFAULT_XML macro.
	epatch "${FILESDIR}"/ibus-m17n-1.3.3-allow-override-xml-path.patch
	epatch "${FILESDIR}"/ibus-m17n-1.3.3-Fix-a-crash-and-add-some-warning-log-message.patch

	# Build ibus-engine-m17n for the host platform.
	(env -i ./configure && \
	 env -i make CFLAGS=-DDEFAULT_XML='\"./src/default.xml\"') || die
	# Obtain the XML output by running the binary.
	src/ibus-engine-m17n --xml > output.xml || die
	# Sanity checks.
	grep m17n:ar:kbd output.xml    || die  # in m17n-db
	grep m17n:mr:itrans output.xml || die  # in m17n-db-contrib
	# Clean up.
	make distclean || die
}

src_configure() {
	econf $(use_enable nls) || die
}

src_compile() {
	emake || die
	# Rewrite m17n.xml using the XML output.
	# input_methods.txt comes from chromeos-base/chromeos-chrome-9999.ebuild
	ASSETSIMDIR="${SYSROOT}"/usr/share/chromeos-assets/input_methods

	LIST="${ASSETSIMDIR}"/input_methods.txt
	if [ -f ${LIST} ] ; then
		python "${ASSETSIMDIR}"/filter.py < output.xml \
		--whitelist="${LIST}" \
		--rewrite=src/m17n.xml || die
	else
		# TODO(yusukes): Remove ibus_input_methods.txt support in R20.
		LIST_OLD="${ASSETSIMDIR}"/ibus_input_methods.txt
		if [ -f ${LIST_OLD} ] ; then
		   python "${ASSETSIMDIR}"/filter.py < output.xml \
		   --whitelist="${LIST_OLD}" \
		   --rewrite=src/m17n.xml || die
		fi
	fi

	# Remove spaces from the XML to reduce file size from ~4k to ~3k.
	# You can make it readable by 'xmllint --format' (on a target machine).
	mv src/m17n.xml "${T}"/ || die
	xmllint --noblanks "${T}"/m17n.xml > src/m17n.xml || die
	# Sanity checks.
	grep m17n:ar:kbd src/m17n.xml 2>&1 > /dev/null \
	    || die  # in m17n-db, whitelisted
	grep m17n:mr:itrans src/m17n.xml 2>&1 > /dev/null \
	    || die  # in m17n-db-contrib, whitelisted
}

src_install() {
	emake DESTDIR="${D}" install || die
	rm -rf "${D}/usr/libexec/ibus-setup-m17n" || die
	rm -rf "${D}/usr/share/ibus-m17n/icons" || die

	# We should not delete default.xml in setup/.
	rm -rf "${D}/usr/share/ibus-m17n/setup/ibus-m17n-preferences.ui" \
	    || die

	dodoc AUTHORS ChangeLog NEWS README
}

pkg_postinst() {
	ewarn "This package is very experimental, please report your bugs to"
	ewarn "http://ibus.googlecode.com/issues/list"
	elog
	elog "You should run ibus-setup and enable IM Engines you want to use!"
	elog
}
