# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="Sony vector math library."
HOMEPAGE="http://www.bulletphysics.com/Bullet/phpBB2/viewforum.php?f=18"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${PN}-svn-${PV}.tar.gz"
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE=""

src_install() {
	insinto /usr/include/vectormath/scalar/cpp
	doins "${S}/include/vectormath/scalar/cpp/mat_aos.h"
	doins "${S}/include/vectormath/scalar/cpp/quat_aos.h"
	doins "${S}/include/vectormath/scalar/cpp/vec_aos.h"
	doins "${S}/include/vectormath/scalar/cpp/vectormath_aos.h"
}
