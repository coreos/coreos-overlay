# Copyright 2012 The Chromium OS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="8dbe89e0e08d5e5adeb4f414e8f284dbf0c17b80"
CROS_WORKON_TREE="3d6b0d529e066591245a40e40731b7e2b20d3b5c"
CROS_WORKON_PROJECT="chromiumos/third_party/tlsdate"

inherit autotools flag-o-matic toolchain-funcs cros-workon

DESCRIPTION="Update local time over HTTPS"
HOMEPAGE="https://github.com/ioerror/tlsdate"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="+dbus"

DEPEND="dev-libs/openssl
	dbus? ( sys-apps/dbus )"
RDEPEND="${DEPEND}"

src_prepare() {
	eautoreconf
}

src_configure() {
	# Our unprivileged group is called "tlsdate"
	econf \
		$(use_enable dbus) \
		--with-unpriv-user=tlsdate \
		--with-unpriv-group=tlsdate \
		--with-dbus-user=tlsdate-dbus \
		--with-dbus-group=tlsdate-dbus
}

src_compile() {
	tc-export CC
	emake CFLAGS="-Wall ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}"
}

src_install() {
	default
	insinto /etc/tlsdate
	doins "${FILESDIR}/tlsdated.conf"
	insinto /etc/dbus-1/system.d
	doins "${FILESDIR}/org.torproject.tlsdate.conf"

	systemd_dounit "${FILESDIR}/tlsdate.service"
	systemd_enable_service default.target tlsdate.service
}
