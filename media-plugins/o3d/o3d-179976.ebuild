# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

# added eutils to patch
inherit toolchain-funcs eutils flag-o-matic

DESCRIPTION="O3D Plugin"
HOMEPAGE="http://code.google.com/p/o3d/"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${PN}-svn-${PV}.tar.gz"
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="opengl opengles"
DEPEND="dev-libs/nss
	media-libs/fontconfig
	opengl? ( media-libs/glew )
	net-misc/curl
	opengles? ( virtual/opengles x11-drivers/opengles-headers )
	x11-libs/cairo
	x11-libs/gtk+"
RDEPEND="${DEPEND}"

set_build_defines() {
	# Prevents gclient from updating self.
	export DEPOT_TOOLS_UPDATE=0
	export EGCLIENT="${EGCLIENT:-/home/$(whoami)/depot_tools/gclient}"
}

src_prepare() {
	set_build_defines

	if use x86; then
		# TODO(piman): switch to GLES backend
		GYP_DEFINES="target_arch=ia32";
	elif use arm; then
		GYP_DEFINES="target_arch=arm"
	elif use amd64; then
		GYP_DEFINES="target_arch=x64"
	else
		die "unsupported arch: ${ARCH}"
	fi
	if [[ -n "${ROOT}" && "${ROOT}" != "/" ]]; then
		GYP_DEFINES="$GYP_DEFINES sysroot=$ROOT"
	fi
	export GYP_DEFINES="$GYP_DEFINES chromeos=1 plugin_interface=ppapi p2p_apis=0 os_posix=1 plugin_branding=gtpo3d $BUILD_DEFINES"
	# ebuild uses emake, ensure GYP creates Makefiles (and not .ninja)
	# and we don't have external environment variables leak in
	export GYP_GENERATORS=make
	unset GYP_GENERATOR_FLAGS
	unset BUILD_OUT
	unset builddir_name
	epatch "${FILESDIR}"/gcc-4_7.patch

	${EGCLIENT} runhooks || die
}

src_compile() {
	use arm && append-flags -Wa,-mimplicit-it=always
	append-cxxflags $(test-flags-CC -Wno-error=unused-but-set-variable)
	tc-export AR AS LD NM RANLIB CC CXX STRIP

	emake BUILDTYPE=Release ppo3dautoplugin || die
}

src_install() {
	local destdir=/opt/google/o3d
	local chromepepperdir=/opt/google/chrome/pepper

	dodir ${destdir}
	exeinto ${destdir}
	doexe out/Release/libppo3dautoplugin.so || die
	dodir ${chromepepperdir}
	dosym ${destdir}/libppo3dautoplugin.so ${chromepepperdir}/ || die
}
