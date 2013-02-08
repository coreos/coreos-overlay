# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

# Hand-install (with cros_package_to_live) on a device to make it an
# endpoint in the cell testbed

DESCRIPTION="Performance testing endpoint for cellular performance tests"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RDEPEND="net-misc/iperf"
DEPEND=""

S=${WORKDIR}

src_install() {
	insinto /etc/init
	doins "${FILESDIR}"/celltest-perf-endpoint-{network,iperf,webserver}.conf

	dobin "${FILESDIR}"/celltest-perf-endpoint-webserver
}
