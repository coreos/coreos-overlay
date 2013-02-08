# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# build with
# ACCEPT=~arm emerge-$BOARD flashbench
EAPI="4"

inherit eutils

if [[ ${PV} == "9999" ]] ; then
	EGIT_REPO_URI="https://github.com/bradfa/flashbench.git"
	EGIT_MASTER="dev"
	inherit git-2
	SRC_URI=""
else
	# This is a snapshot of "dev" branch
	VER="dev-20121121"
	SRC_URI="https://storage.cloud.google.com/chromeos-localmirror/distfiles/flashbench-${VER}.tar.gz"
	S=${WORKDIR}/${PN}-${VER}
fi

DESCRIPTION="Flash Storage Benchmark"
HOMEPAGE="https://github.com/bradfa/flashbench"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

src_prepare() {
	epatch "${FILESDIR}"/flashbench-20121121-Makefile-install.patch
}

src_configure() {
	tc-export CC
}
