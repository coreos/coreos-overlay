#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=5
inherit git-2

DESCRIPTION="oem suite for rackspace images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

EGIT_REPO_URI="https://github.com/coreos/nova-agent-container.git"

src_install() {
	rsync --exclude=".git" -aq ${S}/ ${D}/nova-agent/
	rsync -aq ${FILESDIR}/systemd/ ${D}/system/
	dobin "${FILESDIR}"/ifconfig.sh
	dobin "${FILESDIR}"/net.sh

	exeinto "/"
	doexe ${FILESDIR}/run.sh
}
