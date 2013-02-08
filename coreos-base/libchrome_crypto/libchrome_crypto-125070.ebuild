# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# See logic for ${PV} behavior in the libchrome ebuild.

# XXX: Yes, this hits svn rev 125110 instead of rev 125070, but
#      that is correct.  See above note.

EAPI="4"
CROS_WORKON_COMMIT="dee4850a6eb7f90c236846ad9beebed23df76d4f"
CROS_WORKON_PROJECT="chromium/src/crypto"

inherit cros-workon cros-debug toolchain-funcs scons-utils

DESCRIPTION="Chrome crypto/ library extracted for use on Chrome OS"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RDEPEND="chromeos-base/libchrome:${PV}[cros-debug=]
	dev-libs/nss"
DEPEND="${RDEPEND}
	dev-cpp/gtest"

src_prepare() {
	ln -s "${S}" "${WORKDIR}/crypto" &> /dev/null
	cp -p "${FILESDIR}/SConstruct" "${S}" || die
	epatch "${FILESDIR}/memory_annotation.patch"
}

src_compile() {
	tc-export AR CXX PKG_CONFIG RANLIB
	cros-debug-add-NDEBUG

	BASE_VER=${PV} escons || die
}

src_install() {
	dolib.a libchrome_crypto.a

	insinto /usr/include/crypto
	doins nss_util{,_internal}.h rsa_private_key.h \
		signature_{creator,verifier}.h crypto_export.h
}
