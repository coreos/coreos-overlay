#
# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=5
CROS_WORKON_PROJECT="coreos/etcd-starter"
CROS_WORKON_LOCALNAME="etcd-starter"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/etcd-starter"
inherit coreos-go cros-workon systemd

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="e6265b3a028225a8b57ddbfc3ad1992abc89974f" # v0.0.2
    KEYWORDS="amd64"
fi

DESCRIPTION="etcd-starter"
HOMEPAGE="https://github.com/coreos/etcd-starter"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE="etcd_protocols_1 etcd_protocols_2"

REQUIRED_USE="|| ( etcd_protocols_1 etcd_protocols_2 )"

DEPEND=">=dev-lang/go-1.2"

src_compile() {
	go_build "${COREOS_GO_PACKAGE}"
}

src_install() {
	local libexec="libexec/etcd/internal_versions"

	if use etcd_protocols_1 && use etcd_protocols_2; then
		dobin "${WORKDIR}/gopath/bin/${PN}"
		dosym "${PN}" /usr/bin/etcd
	elif use etcd_protocols_1; then
		dosym "../${libexec}/1/etcd" /usr/bin/etcd
	elif use etcd_protocols_2; then
		dosym "../${libexec}/2/etcd" /usr/bin/etcd
	fi

	systemd_dounit "${FILESDIR}"/etcd.service
	systemd_dotmpfilesd "${FILESDIR}"/etcd.conf
}
