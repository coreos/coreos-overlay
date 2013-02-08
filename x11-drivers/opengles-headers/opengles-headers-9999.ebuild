# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_PROJECT="chromiumos/third_party/khronos"

inherit cros-workon

DESCRIPTION="OpenGL|ES headers."
HOMEPAGE="http://www.khronos.org/opengles/2_X/"
SRC_URI=""
LICENSE="SGI-B-2.0"
SLOT="0"
KEYWORDS="~x86 ~arm"
IUSE=""

RDEPEND=""
DEPEND=""

CROS_WORKON_LOCALNAME="khronos"

src_install() {
	# headers
	insinto /usr/include/EGL
	doins "${S}/include/EGL/egl.h"
	doins "${S}/include/EGL/eglplatform.h"
	doins "${S}/include/EGL/eglext.h"
	insinto /usr/include/KHR
	doins "${S}/include/KHR/khrplatform.h"
	insinto /usr/include/GLES
	doins "${S}/include/GLES/gl.h"
	doins "${S}/include/GLES/glext.h"
	doins "${S}/include/GLES/glplatform.h"
	insinto /usr/include/GLES2
	doins "${S}/include/GLES2/gl2.h"
	doins "${S}/include/GLES2/gl2ext.h"
	doins "${S}/include/GLES2/gl2platform.h"
}
