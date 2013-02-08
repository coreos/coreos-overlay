# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-libs/mesa/mesa-7.9.ebuild,v 1.3 2010/12/05 17:19:14 arfrever Exp $
CROS_WORKON_COMMIT="fc2cf1403860ebc8769dfa36cf7dcdebc1baa305"
CROS_WORKON_TREE="9fac2f50764a0dd604f1d1c3458343cedc871d1d"

EAPI=4

EGIT_REPO_URI="git://anongit.freedesktop.org/mesa/mesa"
CROS_WORKON_PROJECT="chromiumos/third_party/mesa"
CROS_WORKON_LOCALNAME="../third_party/mesa"

if [[ ${PV} = 9999* ]]; then
	GIT_ECLASS="git-2"
	EXPERIMENTAL="true"
fi

inherit base autotools multilib flag-o-matic python toolchain-funcs ${GIT_ECLASS} cros-workon

OPENGL_DIR="xorg-x11"

MY_PN="${PN/m/M}"
MY_P="${MY_PN}-${PV/_/-}"
MY_SRC_P="${MY_PN}Lib-${PV/_/-}"

FOLDER="${PV/_rc*/}"
[[ ${PV/_rc*/} == ${PV} ]] || FOLDER+="/RC"

DESCRIPTION="OpenGL-like graphic library for Linux"
HOMEPAGE="http://mesa3d.sourceforge.net/"

#SRC_PATCHES="mirror://gentoo/${P}-gentoo-patches-01.tar.bz2"
if [[ $PV = 9999* ]] || [[ -n ${CROS_WORKON_COMMIT} ]]; then
	SRC_URI="${SRC_PATCHES}"
else
	SRC_URI="ftp://ftp.freedesktop.org/pub/mesa/${FOLDER}/${MY_SRC_P}.tar.bz2
		${SRC_PATCHES}"
fi

# Most of the code is MIT/X11.
# ralloc is LGPL-3
# GLES[2]/gl[2]{,ext,platform}.h are SGI-B-2.0
LICENSE="MIT LGPL-3 SGI-B-2.0"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 mips ppc ppc64 sh sparc x86 x86-fbsd"

INTEL_CARDS="intel"
RADEON_CARDS="radeon"
VIDEO_CARDS="${INTEL_CARDS} ${RADEON_CARDS} mach64 mga nouveau r128 savage sis vmware tdfx via"
for card in ${VIDEO_CARDS}; do
	IUSE_VIDEO_CARDS+=" video_cards_${card}"
done

IUSE="${IUSE_VIDEO_CARDS}
	+classic debug egl +gallium -gbm gles1 gles2 +llvm motif +nptl pic selinux shared-glapi kernel_FreeBSD"

LIBDRM_DEPSTRING=">=x11-libs/libdrm-2.4.31"
# keep correct libdrm and dri2proto dep
# keep blocks in rdepend for binpkg
RDEPEND="
	!<x11-base/xorg-server-1.7
	!<=x11-proto/xf86driproto-2.0.3
	>=x11-proto/dri2proto-2.2
	>=x11-proto/glproto-1.4.11
	dev-libs/expat
	gbm? ( sys-fs/udev )
	x11-libs/libICE
	>=x11-libs/libX11-1.3.99.901
	x11-libs/libXdamage
	x11-libs/libXext
	x11-libs/libXi
	x11-libs/libXmu
	x11-libs/libXxf86vm
	motif? ( x11-libs/openmotif )
	${LIBDRM_DEPSTRING}
"

DEPEND="${RDEPEND}
	=dev-lang/python-2*
	dev-libs/libxml2
	dev-util/pkgconfig
	x11-misc/makedepend
	x11-proto/inputproto
	>=x11-proto/xextproto-7.0.99.1
	x11-proto/xf86driproto
	x11-proto/xf86vidmodeproto
	!arm? ( sys-devel/llvm )
"

S="${WORKDIR}/${MY_P}"

# It is slow without texrels, if someone wants slow
# mesa without texrels +pic use is worth the shot
QA_EXECSTACK="usr/lib*/opengl/xorg-x11/lib/libGL.so*"
QA_WX_LOAD="usr/lib*/opengl/xorg-x11/lib/libGL.so*"

# Think about: ggi, fbcon, no-X configs

pkg_setup() {
	# workaround toc-issue wrt #386545
	use ppc64 && append-flags -mminimal-toc
}

src_prepare() {
	# apply patches
	if [[ ${PV} != 9999* && -n ${SRC_PATCHES} ]]; then
		EPATCH_FORCE="yes" \
		EPATCH_SOURCE="${WORKDIR}/patches" \
		EPATCH_SUFFIX="patch" \
		epatch
	fi
	# FreeBSD 6.* doesn't have posix_memalign().
	if [[ ${CHOST} == *-freebsd6.* ]]; then
		sed -i \
			-e "s/-DHAVE_POSIX_MEMALIGN//" \
			configure.ac || die
	fi

	epatch "${FILESDIR}"/9.0-cross-compile.patch
	epatch "${FILESDIR}"/7.11-mesa-st-no-flush-front.patch
	epatch "${FILESDIR}"/7.11-state_tracker-gallium-fix-crash-with-st_renderbuffer.patch
	epatch "${FILESDIR}"/7.11_p2-pkgconfig.patch
	epatch "${FILESDIR}"/9.0-builtin_function.patch
	epatch "${FILESDIR}"/9.0-force_s3tc_enable.patch
	epatch "${FILESDIR}"/9.0-i965-Allow-the-case-where-multiple-flush-types-are-e.patch
	epatch "${FILESDIR}"/9.0-i965-Make-sure-we-do-render-between-two-hiz-flushes.patch
	epatch "${FILESDIR}"/9.0-Add-builtin-function-cpp.patch
	epatch "${FILESDIR}"/8.1-dead-code-local-hack.patch
	epatch "${FILESDIR}"/8.1-array-overflow.patch
	epatch "${FILESDIR}"/8.1-lastlevel.patch
	epatch "${FILESDIR}"/8.1-disable-guardband.patch
	epatch "${FILESDIR}"/9.0-uniform-array-bounds-check.patch
	epatch "${FILESDIR}"/9.0-Revert-llvmpipe-fix-overflow-bug-in-total-texture-si.patch
	epatch "${FILESDIR}"/9.0-fail-compile-on-bad-uniform-access.patch
	epatch "${FILESDIR}"/9.0-glx-Check-that-swap_buffers_reply-is-non-NULL-before.patch

	base_src_prepare

	eautoreconf
}

src_configure() {
	tc-getPROG PKG_CONFIG pkg-config

	if use !gallium && use !classic; then
		ewarn "You enabled neither classic nor gallium USE flags. No hardware"
		ewarn "drivers will be built."
	fi

	if use classic; then
	# Configurable DRI drivers
		# Intel code
		driver_enable video_cards_intel i915 i965

		# Nouveau code
		driver_enable video_cards_nouveau nouveau

		# ATI code
		driver_enable video_cards_radeon radeon r200
	fi

	if use gallium; then
	# Configurable gallium drivers
		gallium_driver_enable swrast

		# Intel code
		gallium_driver_enable video_cards_intel i915

		# Nouveau code
		gallium_driver_enable video_cards_nouveau nouveau

		# ATI code
		gallium_driver_enable video_cards_radeon r300 r600
	fi

	export LLVM_CONFIG=${SYSROOT}/usr/bin/llvm-config-host

	# --with-driver=dri|xlib|osmesa || do we need osmesa?
	econf \
		--disable-option-checking \
		--with-driver=dri \
		--disable-glu \
		--disable-glut \
		--without-demos \
		--enable-texture-float \
		--enable-xcb \
		$(use_enable llvm llvm-gallium) \
		$(use_enable egl) \
		$(use_enable gbm) \
		$(use_enable gles1) \
		$(use_enable gles2) \
		$(use_enable shared-glapi) \
		$(use_enable gallium) \
		$(use_enable debug) \
		$(use_enable nptl glx-tls) \
		$(use_enable motif glw) \
		$(use_enable motif) \
		$(use_enable !pic asm) \
		--with-dri-drivers=${DRI_DRIVERS} \
		--with-gallium-drivers=${GALLIUM_DRIVERS} \
	        $(use gbm && echo "--with-egl-platforms=drm")
}

src_install() {
	base_src_install

	# Save the glsl-compiler for later use
	if ! tc-is-cross-compiler; then
		dobin "${S}"/src/glsl/glsl_compiler || die
	fi
	# Remove redundant headers
        # GLU and GLUT
	rm -f "${D}"/usr/include/GL/glu*.h || die "Removing GLU and GLUT headers failed."
	# Glew includes
	rm -f "${D}"/usr/include/GL/{glew,glxew,wglew}.h \
		|| die "Removing glew includes failed."

	# Move libGL and others from /usr/lib to /usr/lib/opengl/blah/lib
	# because user can eselect desired GL provider.
	ebegin "Moving libGL and friends for dynamic switching"
		dodir /usr/$(get_libdir)/opengl/${OPENGL_DIR}/{lib,extensions,include}
		local x
		for x in "${D}"/usr/$(get_libdir)/libGL.{la,a,so*}; do
			if [ -f ${x} -o -L ${x} ]; then
				mv -f "${x}" "${D}"/usr/$(get_libdir)/opengl/${OPENGL_DIR}/lib \
					|| die "Failed to move ${x}"
			fi
		done
		for x in "${D}"/usr/include/GL/{gl.h,glx.h,glext.h,glxext.h}; do
			if [ -f ${x} -o -L ${x} ]; then
				mv -f "${x}" "${D}"/usr/$(get_libdir)/opengl/${OPENGL_DIR}/include \
					|| die "Failed to move ${x}"
			fi
		done
	eend $?

	dodir /usr/$(get_libdir)/dri
	insinto "/usr/$(get_libdir)/dri/"
	insopts -m0755
	# install the gallium drivers we use
	local gallium_drivers_files=( i915_dri.so nouveau_dri.so r300_dri.so r600_dri.so swrast_dri.so )
	for x in ${gallium_drivers_files[@]}; do
		if [ -f "${S}/$(get_libdir)/gallium/${x}" ]; then
			doins "${S}/$(get_libdir)/gallium/${x}"
		fi
	done

	# install classic drivers we use
	local classic_drivers_files=( i810_dri.so i965_dri.so nouveau_vieux_dri.so radeon_dri.so r200_dri.so )
	for x in ${classic_drivers_files[@]}; do
		if [ -f "${S}/$(get_libdir)/${x}" ]; then
			doins "${S}/$(get_libdir)/${x}"
		fi
	done

	# Set driconf option to enable S3TC hardware decompression
	insinto "/etc/"
	doins "${FILESDIR}"/drirc
}

pkg_postinst() {
	# Switch to the xorg implementation.
	echo
	eselect opengl set --use-old ${OPENGL_DIR}
}

# $1 - VIDEO_CARDS flag
# other args - names of DRI drivers to enable
driver_enable() {
	case $# in
		# for enabling unconditionally
		1)
			DRI_DRIVERS+=",$1"
			;;
		*)
			if use $1; then
				shift
				for i in $@; do
					DRI_DRIVERS+=",${i}"
				done
			fi
			;;
	esac
}

# $1 - VIDEO_CARDS flag
# other args - names of DRI drivers to enable
gallium_driver_enable() {
	case $# in
		# for enabling unconditionally
		1)
			GALLIUM_DRIVERS+=",$1"
			;;
		*)
			if use $1; then
				shift
				for i in $@; do
					GALLIUM_DRIVERS+=",${i}"
				done
			fi
			;;
	esac
}
