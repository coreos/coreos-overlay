# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="5"

inherit toolchain-glibc

DESCRIPTION="GNU libc6 (also called glibc2) C library"
HOMEPAGE="https://www.gnu.org/software/libc/libc.html"

LICENSE="LGPL-2.1+ BSD HPND ISC inner-net rc PCRE"
KEYWORDS="alpha amd64 ~arm arm64 ~hppa ia64 ~m68k ~mips ppc ppc64 ~s390 ~sh sparc x86"
RESTRICT="strip" # strip ourself #46186
EMULTILIB_PKG="true"

# Configuration variables
RELEASE_VER=""
case ${PV} in
9999*)
	EGIT_REPO_URIS="git://sourceware.org/git/glibc.git"
	EGIT_SOURCEDIRS="${S}"
	inherit git-2
	;;
*)
	RELEASE_VER=${PV}
	;;
esac
GCC_BOOTSTRAP_VER="4.7.3-r1"
# patches live at https://sources.gentoo.org/cgi-bin/viewvc.cgi/gentoo/src/patchsets/glibc/
PATCH_VER="15"                                 # Gentoo patchset
: ${NPTL_KERN_VER:="2.6.32"}                   # min kernel version nptl requires

GLIBC_PATCH_EXCLUDE+=" 0005_all_sys-types.h-drop-sys-sysmacros.h-include.patch"

IUSE="audit caps debug gd hardened multilib nscd +rpc selinux systemtap profile suid vanilla headers-only"

# Here's how the cross-compile logic breaks down ...
#  CTARGET - machine that will target the binaries
#  CHOST   - machine that will host the binaries
#  CBUILD  - machine that will build the binaries
# If CTARGET != CHOST, it means you want a libc for cross-compiling.
# If CHOST != CBUILD, it means you want to cross-compile the libc.
#  CBUILD = CHOST = CTARGET    - native build/install
#  CBUILD != (CHOST = CTARGET) - cross-compile a native build
#  (CBUILD = CHOST) != CTARGET - libc for cross-compiler
#  CBUILD != CHOST != CTARGET  - cross-compile a libc for a cross-compiler
# For install paths:
#  CHOST = CTARGET  - install into /
#  CHOST != CTARGET - install into /usr/CTARGET/

export CBUILD=${CBUILD:-${CHOST}}
export CTARGET=${CTARGET:-${CHOST}}
if [[ ${CTARGET} == ${CHOST} ]] ; then
	if [[ ${CATEGORY} == cross-* ]] ; then
		export CTARGET=${CATEGORY#cross-}
	fi
fi

is_crosscompile() {
	[[ ${CHOST} != ${CTARGET} ]]
}

# Why SLOT 2.2 you ask yourself while sippin your tea ?
# Everyone knows 2.2 > 0, duh.
SLOT="2.2"

# General: We need a new-enough binutils/gcc to match upstream baseline.
# arch: we need to make sure our binutils/gcc supports TLS.
COMMON_DEPEND="
	nscd? ( selinux? (
		audit? ( sys-process/audit )
		caps? ( sys-libs/libcap )
	) )
	suid? ( caps? ( sys-libs/libcap ) )
	selinux? ( sys-libs/libselinux )
	systemtap? ( dev-util/systemtap )
"
DEPEND="${COMMON_DEPEND}
	>=app-misc/pax-utils-0.1.10
	!<sys-apps/sandbox-1.6
	!<sys-apps/portage-2.1.2"
RDEPEND="${COMMON_DEPEND}
	!sys-kernel/ps3-sources
	sys-apps/gentoo-functions
	!sys-libs/nss-db"

if [[ ${CATEGORY} == cross-* ]] ; then
	DEPEND+=" !headers-only? (
		>=${CATEGORY}/binutils-2.24
		>=${CATEGORY}/gcc-4.7
	)"
	[[ ${CATEGORY} == *-linux* ]] && DEPEND+=" ${CATEGORY}/linux-headers"
else
	DEPEND+="
		>=sys-devel/binutils-2.24
		>=sys-devel/gcc-4.7
		virtual/os-headers"
	RDEPEND+=" vanilla? ( !sys-libs/timezone-data )"
	PDEPEND+=" !vanilla? ( sys-libs/timezone-data )"
fi

upstream_uris() {
	echo mirror://gnu/glibc/$1 ftp://sourceware.org/pub/glibc/{releases,snapshots}/$1 mirror://gentoo/$1
}
gentoo_uris() {
	local devspace="HTTP~vapier/dist/URI HTTP~dilfridge/distfiles/URI HTTP~tamiko/distfiles/URI HTTP~slyfox/distfiles/URI"
	devspace=${devspace//HTTP/https://dev.gentoo.org/}
	echo mirror://gentoo/$1 ${devspace//URI/$1}
}
SRC_URI=$(
	[[ -z ${EGIT_REPO_URIS} ]] && upstream_uris ${P}.tar.xz
	[[ -n ${PATCH_VER}      ]] && gentoo_uris ${P}-patches-${PATCH_VER}.tar.bz2
)
SRC_URI+=" ${GCC_BOOTSTRAP_VER:+multilib? ( $(gentoo_uris gcc-${GCC_BOOTSTRAP_VER}-multilib-bootstrap.tar.bz2) )}"

src_unpack() {
	[[ -n ${GCC_BOOTSTRAP_VER} ]] && use multilib && unpack gcc-${GCC_BOOTSTRAP_VER}-multilib-bootstrap.tar.bz2

	toolchain-glibc_src_unpack
}

src_prepare() {
	toolchain-glibc_src_prepare

	cd "${S}"

	epatch "${FILESDIR}"/2.19/${PN}-2.19-ia64-gcc-4.8-reloc-hack.patch #503838
	## COREOS: Apply features and fixes missing from the Gentoo patch set.
	epatch "${FILESDIR}"/${PV}/${P}-gshadow-handle-erange.patch
	epatch "${FILESDIR}"/${PV}/${P}-c-utf8-locale.patch

	if use hardened ; then
		# We don't enable these for non-hardened as the output is very terse --
		# it only states that a crash happened.  The default upstream behavior
		# includes backtraces and symbols.
		einfo "Installing Hardened Gentoo SSP and FORTIFY_SOURCE handler"
		cp "${FILESDIR}"/2.20/glibc-2.20-gentoo-stack_chk_fail.c debug/stack_chk_fail.c || die
		cp "${FILESDIR}"/2.25/glibc-2.25-gentoo-chk_fail.c debug/chk_fail.c || die

		if use debug ; then
			# Allow SIGABRT to dump core on non-hardened systems, or when debug is requested.
			sed -i \
				-e '/^CFLAGS-backtrace.c/ iCPPFLAGS-stack_chk_fail.c = -DSSP_SMASH_DUMPS_CORE' \
				-e '/^CFLAGS-backtrace.c/ iCPPFLAGS-chk_fail.c = -DSSP_SMASH_DUMPS_CORE' \
				debug/Makefile || die
		fi
	fi

	case $(gcc-fullversion) in
	4.8.[0-3]|4.9.0)
		eerror "You need to switch to a newer compiler; gcc-4.8.[0-3] and gcc-4.9.0 miscompile"
		eerror "glibc.  See https://bugs.gentoo.org/547420 for details."
		die "need to switch compilers #547420"
		;;
	esac
}

## COREOS: Redefine some eclass-provided functions for local changes.

# For reference, this function has been modified to do:
# - Config files are installed by baselayout, not glibc.
# - Install nscd/systemd stuff in /usr.
src_install() {
	toolchain-glibc_src_install "$@"

	# Work around #627378 on the boards.
	if [[ ${ROOT:-/} =~ ^/build/ ]] ; then
		local libm=("${ED}"/usr/lib*/libm-${PV}.so)
		libm="${libm[0]:${#ED}}"
		if [ -h "${ED}$libm" ] ; then
			rm -f "${ED}$libm"
			mv "${ED}${libm#/usr}" "${ED}$libm"
		fi
	fi

	# Use tmpfiles to put nscd.conf in /etc and create directories.
	insinto /usr/share/baselayout
	if ! in_iuse nscd || use nscd ; then
		doins "${S}"/nscd/nscd.conf || die
		systemd_newtmpfilesd "${FILESDIR}"/nscd-conf.tmpfiles nscd-conf.conf || die
	fi

	# Clean out any default configs.
	rm -rf "${ED}"/etc

	# Restore this one for the SDK.
	test ! -e "${T}"/00glibc || doenvd "${T}"/00glibc
}

# Ignore /dev/pts settings, since the chroot has no control over them.
check_devpts() { : ; }
