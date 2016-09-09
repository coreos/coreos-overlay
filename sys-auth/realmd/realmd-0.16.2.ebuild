# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

AUTOTOOLS_AUTORECONF=1

inherit autotools-utils systemd

DESCRIPTION="DBus service for configuring kerberos and other online identities"
HOMEPAGE="http://cgit.freedesktop.org/realmd/realmd/"
SRC_URI="http://www.freedesktop.org/software/realmd/releases/${P}.tar.gz"

LICENSE="LGPL-2+"
SLOT="0"
KEYWORDS="amd64 x86 arm64"
IUSE="systemd"

DEPEND="sys-auth/polkit[introspection]
	sys-devel/gettext
	dev-libs/glib:2
	net-nds/openldap
	virtual/krb5
	systemd? ( sys-apps/systemd )"
RDEPEND="${DEPEND}"

# The daemon is installed to a private dir under /usr/lib, similar to systemd.
QA_MULTILIB_PATHS="usr/lib/realmd/realmd"

src_prepare() {
	sed -e '/gentoo-release/s/dnl/ /g' -i configure.ac

	autotools-utils_src_prepare
}

src_configure() {
	PKG_CONFIG=/usr/bin/${CHOST}-pkg-config autotools-utils_src_configure \
		$(use_with systemd systemd-journal) \
		--with-systemd-unit-dir=$(systemd_get_unitdir) \
		--with-distro=defaults \
		--disable-doc
}

src_install() {
	systemd_dotmpfilesd "${FILESDIR}/tmpfiles.d/${PN}.conf"
	autotools-utils_src_install DBUS_POLICY_DIR=/usr/share/dbus-1/system.d
}
