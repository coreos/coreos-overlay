#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=4
CROS_WORKON_PROJECT="coreos/etcd"
CROS_WORKON_LOCALNAME="etcd"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_COMMIT="7b289043c7beced434be4334fb909ba0b16b57b1" # hash for tag v0.1.1
inherit toolchain-funcs cros-workon systemd

DESCRIPTION="etcd"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

DEPEND=">=dev-lang/go-1.1"

src_compile() {
	./build
}

src_install() {
	dobin ${S}/${PN}
	dobin ${FILESDIR}/etcd-bootstrap
	dobin ${FILESDIR}/etcd-pre-exec

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
	systemd_dotmpfilesd "${FILESDIR}"/${PN}.conf
}
