# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

DESCRIPTION="Components to support Google Cloud OS Login. This contains bits that belong in USR"
HOMEPAGE="https://github.com/GoogleCloudPlatform/compute-image-packages/tree/master/google_compute_engine_oslogin"
SRC_URI="https://github.com/GoogleCloudPlatform/compute-image-packages/archive/${PV}.tar.gz"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

inherit pam toolchain-funcs

DEPEND="
	net-misc/curl[ssl]
	dev-libs/json-c
	virtual/pam
"

RDEPEND="${DEPEND}"

S=${WORKDIR}/compute-image-packages-${PV}/google_compute_engine_oslogin

src_prepare() {
	eapply -p2 "$FILESDIR/0001-pam_module-use-var-lib-instead-of-var.patch"
	default
}

src_compile() {
	emake CC="$(tc-getCC)" CXX="$(tc-getCXX)" JSON_INCLUDE_PATH="${ROOT%/}/usr/include/json-c"
}

src_install() {
	dolib.so libnss_cache_google-compute-engine-oslogin-1.3.0.so
	dolib.so libnss_google-compute-engine-oslogin-1.3.0.so

	exeinto /usr/libexec
	doexe google_authorized_keys

	dopammod pam_oslogin_admin.so
	dopammod pam_oslogin_login.so

	# config files the base Ignition config will create links to
	insinto /usr/share/google-oslogin
	doins "${FILESDIR}/sshd_config"
	doins "${FILESDIR}/nsswitch.conf"
	doins "${FILESDIR}/pam_sshd"
	doins "${FILESDIR}/oslogin-sudoers"
}
