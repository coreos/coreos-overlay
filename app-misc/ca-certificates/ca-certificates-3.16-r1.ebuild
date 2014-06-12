# Copyright 2014 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5
PYTHON_COMPAT=( python2_7 )
inherit cros-tmpfiles python-any-r1 systemd

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
KEYWORDS="amd64"
IUSE=""

RDEPEND="dev-libs/openssl
	sys-apps/findutils"
DEPEND="${RDEPEND}
	${PYTHON_DEPS}"

sym_to_usr() {
	local l="/etc/ssl/certs/${1##*/}"
	local p="../../../usr/share/${PN}/${1}"
	echo "L	${l}	-	-	-	-	${p}"
}

gen_tmpfiles() {
	local certfile
	echo "d	/etc/ssl		-	-	-	-	-"
	echo "d	/etc/ssl/certs	-	-	-	-	-"
	sym_to_usr ca-certificates.crt
	for certfile in "$@"; do
		sym_to_usr "${certfile}"
	done
	for certfile in "$@"; do
		local certhash=$(openssl x509 -hash -noout -in "${certfile}")
		# This assumes the hashes have no collisions
		local l="/etc/ssl/certs/${certhash}.0"
		local p="${certfile##*/}"
		echo "L	${l}	-	-	-	-	${p}"
	done
}

src_compile() {
	local certdata="${MY_P}/nss/lib/ckfw/builtins/certdata.txt"
	${PYTHON} "${FILESDIR}/certdata2pem.py" "${certdata}" mozilla || die
	cat mozilla/*.pem > ca-certificates.crt || die
	gen_tmpfiles mozilla/*.pem > ${PN}.conf || die
}

src_install() {
	insinto /usr/share/${PN}
	doins ca-certificates.crt
	doins -r mozilla

	dosbin "${FILESDIR}/update-ca-certificates"
	systemd_dounit "${FILESDIR}/clean-ca-certificates.service"
	systemd_dounit "${FILESDIR}/update-ca-certificates.service"
	systemd_enable_service sysinit.target clean-ca-certificates.service
	systemd_enable_service sysinit.target update-ca-certificates.service
	systemd_dotmpfilesd ${PN}.conf

	# Setup initial links in /etc
	dodir /etc/ssl/certs
	tmpfiles_create
}
