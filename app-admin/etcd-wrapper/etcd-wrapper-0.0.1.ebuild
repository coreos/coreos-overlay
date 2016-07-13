#
# Copyright (c) 2016 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=6

DESCRIPTION="etcd Wrapper"
HOMEPAGE="https://github.com/coreos/etcd"
KEYWORDS="amd64 arm64"

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

RDEPEND=app-emulation/rkt

S=${WORKDIR}

src_install() {
	exeinto /usr/lib/coreos
	doexe "${FILESDIR}"/etcd-wrapper
}
