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

gen_tmpfiles() {
	local certfile
	echo "d	/etc/ssl		-	-	-	-	-"
	echo "d	/etc/ssl/certs	-	-	-	-	-"
	for certfile in "$@"; do
		local l="/etc/ssl/certs/${certfile##*/}"
		local p="../../../usr/share/${PN}/${certfile}"
		echo "L	${l}	-	-	-	-	${p}"
	done
}

src_compile() {
	local certdata="${MY_P}/nss/lib/ckfw/builtins/certdata.txt"
	${PYTHON} "${FILESDIR}/certdata2pem.py" "${certdata}" mozilla || die
	gen_tmpfiles mozilla/*.pem > ${PN}.conf || die
}

src_install() {
	insinto /usr/share/${PN}
	doins -r mozilla

	dosbin "${FILESDIR}/update-ca-certificates"
	systemd_dounit "${FILESDIR}/update-ca-certificates.service"
	systemd_enable_service sysinit.target update-ca-certificates.service
	systemd_dotmpfilesd ${PN}.conf

	# Setup initial links in /etc
	dodir /etc/ssl/certs
	tmpfiles_create
	bash "${FILESDIR}/update-ca-certificates" "${D}/etc/ssl/certs" || die
}
