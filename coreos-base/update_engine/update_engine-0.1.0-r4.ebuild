# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/update_engine"
CROS_WORKON_REPO="git://github.com"
AUTOTOOLS_AUTORECONF=1
# TODO: this can be disabled once -I.. is no longer used
AUTOTOOLS_IN_SOURCE_BUILD=1

CROS_WORKON_COMMIT="c88492c39ec0ad81bb26ae6efde7b4fc46a9804a"
KEYWORDS="amd64 arm x86"

inherit autotools-utils flag-o-matic toolchain-funcs cros-workon systemd

DESCRIPTION="CoreOS OS Update Engine"
HOMEPAGE="https://github.com/coreos/update_engine"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
IUSE="cros-debug cros_host -delta_generator symlink-usr"

LIBCHROME_VERS="180609"

RDEPEND="!coreos-base/coreos-installer
	app-arch/bzip2
	coreos-base/coreos-au-key
	coreos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	dev-cpp/gflags
	dev-libs/dbus-glib
	dev-libs/glib
	dev-libs/libxml2
	dev-libs/openssl
	dev-libs/protobuf:=
	dev-util/bsdiff
	net-misc/curl
	sys-fs/e2fsprogs"
DEPEND="dev-cpp/gmock
	dev-cpp/gtest
	${RDEPEND}"

src_configure() {
	# Disable PIE when building for the SDK, this works around a bug that
	# breaks using delta_generator from the update.zip bundle.
	# https://code.google.com/p/chromium/issues/detail?id=394508
	# https://code.google.com/p/chromium/issues/detail?id=394241
	if use cros_host; then
		append-flags -nopie
		append-ldflags -nopie
	fi

	local myeconfargs=(
		$(use_enable cros-debug debug)
		$(use_enable delta_generator)
	)

	autotools-utils_src_configure
}

src_test() {
	if use cros_host; then
		autotools-utils_src_test
	else
		ewarn "Skipping tests on cross-compiled target platform..."
	fi
}

src_install() {
	autotools-utils_src_install

	if use symlink-usr; then
		dosym sbin/coreos-postinst /usr/postinst
	else
		dosym usr/sbin/coreos-postinst /postinst
	fi

	systemd_dounit systemd/update-engine.service
	systemd_dounit systemd/update-engine-stub.service
	systemd_dounit systemd/update-engine-stub.timer

	systemd_enable_service multi-user.target update-engine.service
	systemd_enable_service multi-user.target update-engine-stub.timer

	insinto /usr/share/dbus-1/services
	doins com.coreos.update1.service

	insinto /usr/share/dbus-1/system.d
	doins com.coreos.update1.conf

	# Install rule to remove old UpdateEngine.conf from /etc
	systemd_dotmpfilesd "${FILESDIR}"/update-engine.conf
}
