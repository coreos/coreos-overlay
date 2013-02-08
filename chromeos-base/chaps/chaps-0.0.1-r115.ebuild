# Copyright (C) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE.makefile file.

EAPI="4"
CROS_WORKON_COMMIT="80e2bd82e37b10c72cc5af63ee705379955a4fa1"
CROS_WORKON_TREE="21c1942d9a04e83433ab0a7af6b6ce0346f647c1"
CROS_WORKON_PROJECT="chromiumos/platform/chaps"
CROS_WORKON_OUTOFTREE_BUILD=1

inherit cros-debug cros-workon

DESCRIPTION="PKCS #11 layer over TrouSerS."
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="arm amd64 x86"
IUSE="test"

LIBCHROME_VERS="125070"

RDEPEND="
	app-crypt/trousers
	chromeos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	chromeos-base/libchromeos
	dev-libs/dbus-c++
	dev-libs/openssl
	dev-cpp/gflags"

DEPEND="${RDEPEND}
	dev-cpp/gmock
	test? ( dev-cpp/gtest )
	dev-db/leveldb"

# We only depend on this for the init script.
RDEPEND+="
	chromeos-base/chromeos-init"

src_prepare() {
	cros-workon_src_prepare
}

src_configure() {
	cros-workon_src_configure
}

src_compile() {
	cros-workon_src_compile
}

src_test() {
	cros-workon_src_test
	emake more_tests
}

src_install() {
	dosbin "${OUT}"/chapsd
	dobin "${OUT}"/chaps_client
	dobin "${OUT}"/p11_replay
	dolib.so "${OUT}"/libchaps.so
	# Install D-Bus config file.
	insinto /etc/dbus-1/system.d
	doins org.chromium.Chaps.conf
	# Install D-Bus service file.
	insinto /usr/share/dbus-1/services
	doins org.chromium.Chaps.service
	# Install upstart config file.
	insinto /etc/init
	doins chapsd.conf
	# Install headers for use by clients.
	insinto /usr/include/chaps
	doins login_event_client.h
	insinto /usr/include/chaps/pkcs11
	doins pkcs11/*.h
}

