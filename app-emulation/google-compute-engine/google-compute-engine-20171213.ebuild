# Copyright (c) 2016-2018 CoreOS, Inc. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit eutils

DESCRIPTION="Linux Guest Environment for Google Compute Engine"
HOMEPAGE="https://github.com/GoogleCloudPlatform/compute-image-packages"
SRC_URI="https://github.com/GoogleCloudPlatform/compute-image-packages/archive/${PV}.tar.gz"

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

DEPEND="dev-python/setuptools"

# These dependencies cover all commands called by the scripts.
RDEPEND="
	app-admin/sudo
	dev-python/boto
	dev-python/setuptools
	sys-apps/ethtool
	sys-apps/gawk
	sys-apps/grep
	sys-apps/iproute2
	sys-apps/shadow
"

S="${WORKDIR}/compute-image-packages-${PV}"

src_compile() {
	(cd "${S}" && exec python setup.py build)
}

src_install() {
	(cd "${S}" && exec python setup.py install -O1 --skip-build --root "${D}")
}
