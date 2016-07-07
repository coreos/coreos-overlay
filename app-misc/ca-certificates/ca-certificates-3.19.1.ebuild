# Copyright 2014 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5
PYTHON_COMPAT=( python2_7 )
inherit python-any-r1 systemd

RTM_NAME="NSS_${PV//./_}_RTM"
MY_PN="nss"
MY_P="${MY_PN}-${PV}"
S="${WORKDIR}"

DESCRIPTION="Mozilla's CA Certificate Store"
HOMEPAGE="http://www.mozilla.org/en-US/about/governance/policies/security-group/certs/"
SRC_URI="ftp://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/${RTM_NAME}/src/${MY_P}.tar.gz"

# NSS is licensed under the MPL, files/certdata2pem.py is GPL
LICENSE="MPL-2.0 GPL-2"
SLOT="0"
KEYWORDS="amd64 arm64"
IUSE=""

RDEPEND="dev-libs/openssl
	sys-apps/findutils
	sys-apps/systemd"
DEPEND="${RDEPEND}
	${PYTHON_DEPS}"

pkg_setup() {
	python-any-r1_pkg_setup

	# Deal with the case where older ca-certificates installed a
	# dir here, but newer one installs symlinks.  Portage will
	# barf when you try to transition file types.
	# This trick is stolen from sys-libs/timezone-data
	if cd "${EROOT}"/usr/share/${PN} 2>/dev/null ; then
		# In case of a failed upgrade, clean up the symlinks #506570
		if [ -L .gentoo-upgrade ] ; then
			rm -rf mozilla .gentoo-upgrade
		fi
		if [ -d mozilla ] ; then
			rm -rf .gentoo-upgrade #487192
			mv mozilla .gentoo-upgrade || die
			ln -s .gentoo-upgrade mozilla || die
		fi
	fi
}

gen_hash_links() {
	local certfile certhash
	for certfile in "$@"; do
		certhash=$(openssl x509 -hash -noout -in "${certfile}") || die
		# This assumes the hashes have no collisions
		ln -s "${certfile}" "${certhash}.0" || die
	done
}

gen_tmpfiles() {
	local certfile
	echo "d	/etc/ssl		-	-	-	-	-"
	echo "d	/etc/ssl/certs	-	-	-	-	-"
	for certfile in "$@"; do
		local l="/etc/ssl/certs/${certfile}"
		local p="../../../usr/share/${PN}/${certfile}"
		echo "L	${l}	-	-	-	-	${p}"
	done
}

src_compile() {
	local certdata="${MY_P}/nss/lib/ckfw/builtins/certdata.txt"
	${PYTHON} "${FILESDIR}/certdata2pem.py" "${certdata}" certs || die

	cd certs || die
	gen_hash_links *.pem
	cat *.pem > ca-certificates.crt || die
	gen_tmpfiles * > "${S}/${PN}.conf" || die
}

src_install() {
	insinto /usr/share/${PN}
	doins certs/*

	# for compatibility with older directory structure
	dosym . /usr/share/${PN}/mozilla

	dosbin "${FILESDIR}/update-ca-certificates"
	systemd_dounit "${FILESDIR}/clean-ca-certificates.service"
	systemd_dounit "${FILESDIR}/update-ca-certificates.service"
	systemd_enable_service sysinit.target clean-ca-certificates.service
	systemd_enable_service sysinit.target update-ca-certificates.service
	systemd_dotmpfilesd ${PN}.conf

	# Setup initial links in /etc
	dodir /etc/ssl/certs
	systemd-tmpfiles --root="${D}" --create
}

pkg_postinst() {
	rm -rf "${EROOT}"/usr/share/${PN}/.gentoo-upgrade
}
