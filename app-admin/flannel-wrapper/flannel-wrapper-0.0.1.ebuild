# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit systemd

DESCRIPTION="flannel (System Application Container)"
HOMEPAGE="https://github.com/coreos/flannel"

KEYWORDS="amd64"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND="
	!app-admin/flannel
	>=app-emulation/rkt-1.9.1[rkt_stage1_fly]
"

S="$WORKDIR"

src_install() {
	exeinto /usr/lib/coreos
	doexe "${FILESDIR}"/flannel-wrapper

	systemd_dounit "${FILESDIR}"/flanneld.service
	systemd_dounit "${FILESDIR}"/flannel-docker-opts.service

	insinto /usr/lib/systemd/network
	doins "${FILESDIR}"/50-flannel.network
}
