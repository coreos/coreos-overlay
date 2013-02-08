# Copyright 2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/opencryptoki/opencryptoki-2.2.8.ebuild,v 1.1 2009/06/28 10:48:58 arfrever Exp $
EAPI="2"

CROS_WORKON_COMMIT="a8cef4ae1b31664c9f59c509f6e79c1fc03f4bc4"
CROS_WORKON_TREE="8f71d13ce4e947d8a631fc03d8bfd4f63587d3a7"
inherit cros-workon autotools eutils multilib toolchain-funcs

DESCRIPTION="PKCS#11 provider for IBM cryptographic hardware"
HOMEPAGE="http://sourceforge.net/projects/opencryptoki"
LICENSE="CPL-0.5"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="tpmtok"
CROS_WORKON_PROJECT="chromiumos/third_party/opencryptoki"

RDEPEND="tpmtok? ( app-crypt/trousers )
	 dev-libs/openssl"
DEPEND="${RDEPEND}"

src_prepare() {
	eautoreconf
}

src_configure() {
	econf $(use_enable tpmtok)
}

src_install() {
	emake install DESTDIR="${D}" || die "emake install failed"

	# tpmtoken_* binaries expect to find the libraries in /usr/lib/.
	dosym opencryptoki/stdll/libpkcs11_sw.so.0.0.0 "/usr/$(get_libdir)/libpkcs11_sw.so"
	dosym opencryptoki/stdll/libpkcs11_tpm.so.0.0.0 "/usr/$(get_libdir)/libpkcs11_tpm.so"
	dosym opencryptoki/libopencryptoki.so.0.0.0 "/usr/$(get_libdir)/libopencryptoki.so"
	dosym opencryptoki/stdll/libpkcs11_sw.so.0.0.0 "/usr/$(get_libdir)/libpkcs11_sw.so.0"
	dosym opencryptoki/stdll/libpkcs11_tpm.so.0.0.0 "/usr/$(get_libdir)/libpkcs11_tpm.so.0"
	dosym opencryptoki/libopencryptoki.so.0.0.0 "/usr/$(get_libdir)/libopencryptoki.so.0"
}
