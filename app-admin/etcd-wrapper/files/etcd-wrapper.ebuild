#
# Copyright (c) 2016 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=6

inherit systemd

DESCRIPTION="etcd (System Application Container)"
HOMEPAGE="https://github.com/coreos/etcd"
KEYWORDS="amd64"

LICENSE="Apache-2.0"
IUSE=""

MY_PV=${PV}
MY_PV=${MY_PV/_alpha/-alpha.}
MY_PV=${MY_PV/_beta/-beta.}

case ${MY_PV} in
2*)
	SLOT="2"
	;;
3*)
	SLOT="3"
	;;
*)
	die "Unknown etcd version ${MY_PV}"
	;;
esac

SRC_URI="https://coreos.com/dist/pubkeys/app-signing-pubkey.gpg -> etcd-v${MY_PV}_pubkey.gpg"

IMG_FILENAME="etcd-v${MY_PV}-linux-${ARCH}.aci"
PUBKEY_FILENAME="etcd-v${MY_PV}_pubkey.gpg"
IMG_PREFIX="coreos.com/etcd"
IMG_PUBKEY="${DISTDIR}/${PUBKEY_FILENAME}"
RKT="rkt"
VERSION_DIR=/usr/lib/coreos/versions
VERSION_DIR_VAR=/var/lib/coreos/versions
IMG_VERSION_FILE="${VERSION_DIR}/etcd${SLOT}"
IMG_VERSION_FILE_VAR="${VERSION_DIR_VAR}/etcd${SLOT}"
IMG_VERSION_DIR="${VERSION_DIR}/etcd${SLOT}.d"
ENV_PREFIX="ETCD"

DEPEND=""
RDEPEND=">=app-emulation/rkt-1.9.1[rkt_stage1_fly]"

S=${WORKDIR}

pkg_preinst() {
	echo ${ENV_PREFIX}_IMG="https://github.com/coreos/etcd/releases/download/v${MY_PV}/etcd-v${MY_PV}-linux-${ARCH}.aci" >> "${D}/${IMG_VERSION_FILE}"
	echo ${ENV_PREFIX}_IMG_USER=etcd >> "${D}/${IMG_VERSION_FILE}"
	echo ${ENV_PREFIX}_DATA_DIR=/var/lib/etcd${SLOT} >> "${D}/${IMG_VERSION_FILE}"
	echo ${ENV_PREFIX}_IMG_PREFIX=${IMG_PREFIX} >> "${D}/${IMG_VERSION_FILE}"
	echo ${ENV_PREFIX}_IMG_PUBKEY=${IMG_VERSION_DIR}/${PUBKEY_FILENAME} >> "${D}/${IMG_VERSION_FILE}"
}

src_install() {
	insinto "${IMG_VERSION_DIR}"
	doins "${DISTDIR}/${PUBKEY_FILENAME}"

	local wrapper_file="${TMP}/etcd${SLOT}-wrapper"
	cat	${FILESDIR}/etcd-wrapper.template > ${wrapper_file}
	sed -i ${wrapper_file} -e \
		"s,{{SOURCE_FILES}},${IMG_VERSION_FILE} ${IMG_VERSION_FILE_VAR},g" \
		|| die
	dobin ${wrapper_file} 

	local service_file="${TMP}/etcd${SLOT}-wrapper.service"
	cat "${FILESDIR}/etcd-wrapper.service.template" > ${service_file}
	cat ${service_file}
	sed -i ${service_file} -e \
		"s,{{SLOT}},${SLOT},g" \
		|| die
	sed -i ${service_file} -e \
		"s,{{SOURCE_FILES}},${IMG_VERSION_FILE} ${IMG_VERSION_FILE_VAR},g" \
		|| die
	systemd_dounit "${service_file}"
}
