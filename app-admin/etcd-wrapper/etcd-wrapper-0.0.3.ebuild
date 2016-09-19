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
SLOT=0

DEPEND=""
RDEPEND=">=app-emulation/rkt-1.9.1[rkt_stage1_fly]"

S=${WORKDIR}

src_install() {
	exeinto /usr/lib/coreos
	doexe "${FILESDIR}"/etcd-wrapper

  	systemd_dounit "${FILESDIR}"/etcd-member.service
}
