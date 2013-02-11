# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/toolchain.eclass,v 1.568 2013/01/24 01:27:27 vapier Exp $
#
# Maintainer: Toolchain Ninjas <toolchain@gentoo.org>

#---->> eclass stuff <<----
HOMEPAGE="http://gcc.gnu.org/"
LICENSE="GPL-2 LGPL-2.1"
RESTRICT="strip" # cross-compilers need controlled stripping

inherit eutils versionator libtool toolchain-funcs flag-o-matic gnuconfig multilib fixheadtails pax-utils

if [[ ${PV} == *_pre9999* ]] ; then
	EGIT_REPO_URI="git://gcc.gnu.org/git/gcc.git"
	# naming style:
	# gcc-4.7.1_pre9999 -> gcc-4_7-branch
	#  Note that the micro version is required or lots of stuff will break.
	#  To checkout master set gcc_LIVE_BRANCH="master" in the ebuild before
	#  inheriting this eclass.
	EGIT_BRANCH="${PN}-${PV%.?_pre9999}-branch"
	EGIT_BRANCH=${EGIT_BRANCH//./_}
	inherit git-2
fi

EXPORT_FUNCTIONS pkg_setup src_unpack src_compile src_test src_install pkg_postinst pkg_postrm
DESCRIPTION="The GNU Compiler Collection"

FEATURES=${FEATURES/multilib-strict/}
#----<< eclass stuff >>----


#---->> globals <<----
export CTARGET=${CTARGET:-${CHOST}}
if [[ ${CTARGET} = ${CHOST} ]] ; then
	if [[ ${CATEGORY/cross-} != ${CATEGORY} ]] ; then
		export CTARGET=${CATEGORY/cross-}
	fi
fi
: ${TARGET_ABI:=${ABI}}
: ${TARGET_MULTILIB_ABIS:=${MULTILIB_ABIS}}
: ${TARGET_DEFAULT_ABI:=${DEFAULT_ABI}}

is_crosscompile() {
	[[ ${CHOST} != ${CTARGET} ]]
}

tc_version_is_at_least() { version_is_at_least "$1" "${2:-${GCC_RELEASE_VER}}" ; }

GCC_PV=${TOOLCHAIN_GCC_PV:-${PV}}
GCC_PVR=${GCC_PV}
[[ ${PR} != "r0" ]] && GCC_PVR=${GCC_PVR}-${PR}
GCC_RELEASE_VER=$(get_version_component_range 1-3 ${GCC_PV})
GCC_BRANCH_VER=$(get_version_component_range 1-2 ${GCC_PV})
GCCMAJOR=$(get_version_component_range 1 ${GCC_PV})
GCCMINOR=$(get_version_component_range 2 ${GCC_PV})
GCCMICRO=$(get_version_component_range 3 ${GCC_PV})
[[ ${BRANCH_UPDATE-notset} == "notset" ]] && BRANCH_UPDATE=$(get_version_component_range 4 ${GCC_PV})

# According to gcc/c-cppbuiltin.c, GCC_CONFIG_VER MUST match this regex.
# ([^0-9]*-)?[0-9]+[.][0-9]+([.][0-9]+)?([- ].*)?
GCC_CONFIG_VER=${GCC_CONFIG_VER:-$(replace_version_separator 3 '-' ${GCC_PV})}

# Pre-release support
if [[ ${GCC_PV} != ${GCC_PV/_pre/-} ]] ; then
	PRERELEASE=${GCC_PV/_pre/-}
fi
# make _alpha and _beta ebuilds automatically use a snapshot
if [[ ${GCC_PV} == *_alpha* ]] ; then
	SNAPSHOT=${GCC_BRANCH_VER}-${GCC_PV##*_alpha}
elif [[ ${GCC_PV} == *_beta* ]] ; then
	SNAPSHOT=${GCC_BRANCH_VER}-${GCC_PV##*_beta}
elif [[ ${GCC_PV} == *_rc* ]] ; then
	SNAPSHOT=${GCC_PV%_rc*}-RC-${GCC_PV##*_rc}
fi
export GCC_FILESDIR=${GCC_FILESDIR:-${FILESDIR}}

PREFIX=${TOOLCHAIN_PREFIX:-/usr}

if tc_version_is_at_least 3.4.0 ; then
	LIBPATH=${TOOLCHAIN_LIBPATH:-${PREFIX}/lib/gcc/${CTARGET}/${GCC_CONFIG_VER}}
else
	LIBPATH=${TOOLCHAIN_LIBPATH:-${PREFIX}/lib/gcc-lib/${CTARGET}/${GCC_CONFIG_VER}}
fi
INCLUDEPATH=${TOOLCHAIN_INCLUDEPATH:-${LIBPATH}/include}
if is_crosscompile ; then
	BINPATH=${TOOLCHAIN_BINPATH:-${PREFIX}/${CHOST}/${CTARGET}/gcc-bin/${GCC_CONFIG_VER}}
else
	BINPATH=${TOOLCHAIN_BINPATH:-${PREFIX}/${CTARGET}/gcc-bin/${GCC_CONFIG_VER}}
fi
DATAPATH=${TOOLCHAIN_DATAPATH:-${PREFIX}/share/gcc-data/${CTARGET}/${GCC_CONFIG_VER}}
# Dont install in /usr/include/g++-v3/, but in gcc internal directory.
# We will handle /usr/include/g++-v3/ with gcc-config ...
STDCXX_INCDIR=${TOOLCHAIN_STDCXX_INCDIR:-${LIBPATH}/include/g++-v${GCC_BRANCH_VER/\.*/}}

#----<< globals >>----


#---->> SLOT+IUSE logic <<----
IUSE="multislot nls nptl test vanilla"

if [[ ${PN} != "kgcc64" && ${PN} != gcc-* ]] ; then
	IUSE+=" altivec cxx fortran"
	[[ -n ${PIE_VER} ]] && IUSE+=" nopie"
	[[ -n ${HTB_VER} ]] && IUSE+=" boundschecking"
	[[ -n ${D_VER}   ]] && IUSE+=" d"
	[[ -n ${SPECS_VER} ]] && IUSE+=" nossp"

	if tc_version_is_at_least 3 ; then
		IUSE+=" doc gcj gtk hardened multilib objc"

		tc_version_is_at_least "4.0" && IUSE+=" objc-gc mudflap"
		tc_version_is_at_least "4.1" && IUSE+=" libssp objc++"
		tc_version_is_at_least "4.2" && IUSE+=" openmp"
		tc_version_is_at_least "4.3" && IUSE+=" fixed-point"
		tc_version_is_at_least "4.4" && IUSE+=" graphite"
		[[ ${GCC_BRANCH_VER} == 4.5 ]] && IUSE+=" lto"
		tc_version_is_at_least "4.7" && IUSE+=" go"
	fi
fi

# Support upgrade paths here or people get pissed
if use multislot ; then
	SLOT="${GCC_CONFIG_VER}"
else
	SLOT="${GCC_BRANCH_VER}"
fi
#----<< SLOT+IUSE logic >>----

#---->> DEPEND <<----

RDEPEND="sys-libs/zlib
	nls? ( sys-devel/gettext )"
if tc_version_is_at_least 3 ; then
	RDEPEND+=" virtual/libiconv"
fi
if tc_version_is_at_least 4 ; then
	GMP_MPFR_DEPS=">=dev-libs/gmp-4.3.2 >=dev-libs/mpfr-2.4.2"
	if tc_version_is_at_least 4.3 ; then
		RDEPEND+=" ${GMP_MPFR_DEPS}"
	elif in_iuse fortran ; then
		RDEPEND+=" fortran? ( ${GMP_MPFR_DEPS} )"
	fi
	if tc_version_is_at_least 4.5 ; then
		RDEPEND+=" >=dev-libs/mpc-0.8.1"
	fi
	in_iuse lto && RDEPEND+=" lto? ( || ( >=dev-libs/elfutils-0.143 dev-libs/libelf ) )"
fi
if in_iuse graphite ; then
	RDEPEND+="
	    graphite? (
	        >=dev-libs/cloog-ppl-0.15.10
	        >=dev-libs/ppl-0.11
	    )"
fi

DEPEND="${RDEPEND}
	>=sys-apps/texinfo-4.8
	>=sys-devel/bison-1.875
	>=sys-devel/flex-2.5.4
	test? (
		>=dev-util/dejagnu-1.4.4
		>=sys-devel/autogen-5.5.4
	)"
if in_iuse gcj ; then
	GCJ_GTK_DEPS="
		x11-libs/libXt
		x11-libs/libX11
		x11-libs/libXtst
		x11-proto/xproto
		x11-proto/xextproto
		=x11-libs/gtk+-2*
		virtual/pkgconfig
		amd64? ( multilib? (
			app-emulation/emul-linux-x86-gtklibs
			app-emulation/emul-linux-x86-xlibs
		) )
	"
	tc_version_is_at_least 3.4 && GCJ_GTK_DEPS+=" x11-libs/pango"
	GCJ_DEPS=">=media-libs/libart_lgpl-2.1"
	tc_version_is_at_least 4.2 && GCJ_DEPS+=" app-arch/zip app-arch/unzip"
	DEPEND+=" gcj? ( gtk? ( ${GCJ_GTK_DEPS} ) ${GCJ_DEPS} )"
fi

PDEPEND=">=sys-devel/gcc-config-1.7"

#----<< DEPEND >>----

#---->> S + SRC_URI essentials <<----

# Set the source directory depending on whether we're using
# a prerelease, snapshot, or release tarball.
S=$(
	if [[ -n ${PRERELEASE} ]] ; then
		echo ${WORKDIR}/gcc-${PRERELEASE}
	elif [[ -n ${SNAPSHOT} ]] ; then
		echo ${WORKDIR}/gcc-${SNAPSHOT}
	else
		echo ${WORKDIR}/gcc-${GCC_RELEASE_VER}
	fi
)

# This function handles the basics of setting the SRC_URI for a gcc ebuild.
# To use, set SRC_URI with:
#
#	SRC_URI="$(get_gcc_src_uri)"
#
# Other than the variables normally set by portage, this function's behavior
# can be altered by setting the following:
#
#	SNAPSHOT
#			If set, this variable signals that we should be using a snapshot
#			of gcc from ftp://sources.redhat.com/pub/gcc/snapshots/. It is
#			expected to be in the format "YYYY-MM-DD". Note that if the ebuild
#			has a _pre suffix, this variable is ignored and the prerelease
#			tarball is used instead.
#
#	BRANCH_UPDATE
#			If set, this variable signals that we should be using the main
#			release tarball (determined by ebuild version) and applying a
#			CVS branch update patch against it. The location of this branch
#			update patch is assumed to be in ${GENTOO_TOOLCHAIN_BASE_URI}.
#			Just like with SNAPSHOT, this variable is ignored if the ebuild
#			has a _pre suffix.
#
#	PATCH_VER
#	PATCH_GCC_VER
#			This should be set to the version of the gentoo patch tarball.
#			The resulting filename of this tarball will be:
#			gcc-${PATCH_GCC_VER:-${GCC_RELEASE_VER}}-patches-${PATCH_VER}.tar.bz2
#
#	PIE_VER
#	PIE_GCC_VER
#			These variables control patching in various updates for the logic
#			controlling Position Independant Executables. PIE_VER is expected
#			to be the version of this patch, and PIE_GCC_VER the gcc version of
#			the patch:
#			An example:
#					PIE_VER="8.7.6.5"
#					PIE_GCC_VER="3.4.0"
#			The resulting filename of this tarball will be:
#			gcc-${PIE_GCC_VER:-${GCC_RELEASE_VER}}-piepatches-v${PIE_VER}.tar.bz2
#
#	SPECS_VER
#	SPECS_GCC_VER
#			This is for the minispecs files included in the hardened gcc-4.x
#			The specs files for hardenedno*, vanilla and for building the "specs" file.
#			SPECS_VER is expected to be the version of this patch, SPECS_GCC_VER
#			the gcc version of the patch.
#			An example:
#					SPECS_VER="8.7.6.5"
#					SPECS_GCC_VER="3.4.0"
#			The resulting filename of this tarball will be:
#			gcc-${SPECS_GCC_VER:-${GCC_RELEASE_VER}}-specs-${SPECS_VER}.tar.bz2
#
#	HTB_VER
#	HTB_GCC_VER
#			These variables control whether or not an ebuild supports Herman
#			ten Brugge's bounds-checking patches. If you want to use a patch
#			for an older gcc version with a new gcc, make sure you set
#			HTB_GCC_VER to that version of gcc.
#
gentoo_urls() {
	local devspace="HTTP~vapier/dist/URI HTTP~dirtyepic/dist/URI
	HTTP~halcy0n/patches/URI HTTP~zorry/patches/gcc/URI"
	devspace=${devspace//HTTP/http:\/\/dev.gentoo.org\/}
	echo mirror://gentoo/$1 ${devspace//URI/$1}
}

get_gcc_src_uri() {
	export PATCH_GCC_VER=${PATCH_GCC_VER:-${GCC_RELEASE_VER}}
	export UCLIBC_GCC_VER=${UCLIBC_GCC_VER:-${PATCH_GCC_VER}}
	export PIE_GCC_VER=${PIE_GCC_VER:-${GCC_RELEASE_VER}}
	export HTB_GCC_VER=${HTB_GCC_VER:-${GCC_RELEASE_VER}}
	export SPECS_GCC_VER=${SPECS_GCC_VER:-${GCC_RELEASE_VER}}

	# Set where to download gcc itself depending on whether we're using a
	# prerelease, snapshot, or release tarball.
	if [[ -n ${PRERELEASE} ]] ; then
		GCC_SRC_URI="ftp://gcc.gnu.org/pub/gcc/prerelease-${PRERELEASE}/gcc-${PRERELEASE}.tar.bz2"
	elif [[ -n ${SNAPSHOT} ]] ; then
		GCC_SRC_URI="ftp://sources.redhat.com/pub/gcc/snapshots/${SNAPSHOT}/gcc-${SNAPSHOT}.tar.bz2"
	elif [[ ${PV} != *9999* ]] ; then
		GCC_SRC_URI="mirror://gnu/gcc/gcc-${GCC_PV}/gcc-${GCC_RELEASE_VER}.tar.bz2"
		# we want all branch updates to be against the main release
		[[ -n ${BRANCH_UPDATE} ]] && \
			GCC_SRC_URI+=" $(gentoo_urls gcc-${GCC_RELEASE_VER}-branch-update-${BRANCH_UPDATE}.patch.bz2)"
	fi

	[[ -n ${UCLIBC_VER} ]] && GCC_SRC_URI+=" $(gentoo_urls gcc-${UCLIBC_GCC_VER}-uclibc-patches-${UCLIBC_VER}.tar.bz2)"
	[[ -n ${PATCH_VER} ]] && GCC_SRC_URI+=" $(gentoo_urls gcc-${PATCH_GCC_VER}-patches-${PATCH_VER}.tar.bz2)"

	# strawberry pie, Cappuccino and a Gauloises (it's a good thing)
	[[ -n ${PIE_VER} ]] && \
		PIE_CORE=${PIE_CORE:-gcc-${PIE_GCC_VER}-piepatches-v${PIE_VER}.tar.bz2} && \
		GCC_SRC_URI+=" $(gentoo_urls ${PIE_CORE})"

	# gcc minispec for the hardened gcc 4 compiler
	[[ -n ${SPECS_VER} ]] && GCC_SRC_URI+=" $(gentoo_urls gcc-${SPECS_GCC_VER}-specs-${SPECS_VER}.tar.bz2)"

	# gcc bounds checking patch
	if [[ -n ${HTB_VER} ]] ; then
		local HTBFILE="bounds-checking-gcc-${HTB_GCC_VER}-${HTB_VER}.patch.bz2"
		GCC_SRC_URI+="
			boundschecking? (
				mirror://sourceforge/boundschecking/${HTBFILE}
				$(gentoo_urls ${HTBFILE})
			)"
	fi

	[[ -n ${D_VER} ]] && GCC_SRC_URI+=" d? ( mirror://sourceforge/dgcc/gdc-${D_VER}-src.tar.bz2 )"

	# >= gcc-4.3 uses ecj.jar and we only add gcj as a use flag under certain
	# conditions
	if [[ ${PN} != "kgcc64" && ${PN} != gcc-* ]] ; then
		if tc_version_is_at_least "4.5" ; then
			GCC_SRC_URI+=" gcj? ( ftp://sourceware.org/pub/java/ecj-4.5.jar )"
		elif tc_version_is_at_least "4.3" ; then
			GCC_SRC_URI+=" gcj? ( ftp://sourceware.org/pub/java/ecj-4.3.jar )"
		fi
	fi

	echo "${GCC_SRC_URI}"
}
SRC_URI=$(get_gcc_src_uri)
#---->> S + SRC_URI essentials >>----


#---->> support checks <<----

# Grab a variable from the build system (taken from linux-info.eclass)
get_make_var() {
	local var=$1 makefile=${2:-${WORKDIR}/build/Makefile}
	echo -e "e:\\n\\t@echo \$(${var})\\ninclude ${makefile}" | \
		r=${makefile%/*} emake --no-print-directory -s -f - 2>/dev/null
}
XGCC() { get_make_var GCC_FOR_TARGET ; }

# The gentoo piessp patches allow for 3 configurations:
# 1) PIE+SSP by default
# 2) PIE by default
# 3) SSP by default
hardened_gcc_works() {
	if [[ $1 == "pie" ]] ; then
		# $gcc_cv_ld_pie is unreliable as it simply take the output of
		# `ld --help | grep -- -pie`, that reports the option in all cases, also if
		# the loader doesn't actually load the resulting executables.
		# To avoid breakage, blacklist FreeBSD here at least
		[[ ${CTARGET} == *-freebsd* ]] && return 1

		want_pie || return 1
		use_if_iuse nopie && return 1
		hardened_gcc_is_stable pie
		return $?
	elif [[ $1 == "ssp" ]] ; then
		[[ -n ${SPECS_VER} ]] || return 1
		use_if_iuse nossp && return 1
		hardened_gcc_is_stable ssp
		return $?
	else
		# laziness ;)
		hardened_gcc_works pie || return 1
		hardened_gcc_works ssp || return 1
		return 0
	fi
}

hardened_gcc_is_stable() {
	local tocheck
	if [[ $1 == "pie" ]] ; then
		if [[ ${CTARGET} == *-uclibc* ]] ; then
			tocheck=${PIE_UCLIBC_STABLE}
		else
			tocheck=${PIE_GLIBC_STABLE}
		fi
	elif [[ $1 == "ssp" ]] ; then
		if [[ ${CTARGET} == *-uclibc* ]] ; then
			tocheck=${SSP_UCLIBC_STABLE}
		else
			tocheck=${SSP_STABLE}
		fi
	else
		die "hardened_gcc_stable needs to be called with pie or ssp"
	fi

	has $(tc-arch) ${tocheck} && return 0
	return 1
}

want_pie() {
	! use hardened && [[ -n ${PIE_VER} ]] && use nopie && return 1
	[[ -n ${PIE_VER} ]] && [[ -n ${SPECS_VER} ]] && return 0
	tc_version_is_at_least 4.3.2 && return 1
	[[ -z ${PIE_VER} ]] && return 1
	use !nopie && return 0
	return 1
}

want_minispecs() {
	if tc_version_is_at_least 4.3.2 && use hardened ; then
		if ! want_pie ; then
			ewarn "PIE_VER or SPECS_VER is not defiend in the GCC ebuild."
		elif use vanilla ; then
			ewarn "You will not get hardened features if you have the vanilla USE-flag."
		elif use nopie && use nossp ; then
			ewarn "You will not get hardened features if you have the nopie and nossp USE-flag."
		elif ! hardened_gcc_works ; then
			ewarn "Your $(tc-arch) arch is not supported."
		else
			return 0
		fi
		ewarn "Hope you know what you are doing. Hardened will not work."
		return 0
	fi
	return 1
}

# This is to make sure we don't accidentally try to enable support for a
# language that doesnt exist. GCC 3.4 supports f77, while 4.0 supports f95, etc.
#
# Also add a hook so special ebuilds (kgcc64) can control which languages
# exactly get enabled
gcc-lang-supported() {
	grep ^language=\"${1}\" "${S}"/gcc/*/config-lang.in > /dev/null || return 1
	[[ -z ${TOOLCHAIN_ALLOWED_LANGS} ]] && return 0
	has $1 ${TOOLCHAIN_ALLOWED_LANGS}
}

#----<< support checks >>----

#---->> specs + env.d logic <<----

# configure to build with the hardened GCC specs as the default
make_gcc_hard() {
	# defaults to enable for all hardened toolchains
	local gcc_hard_flags="-DEFAULT_RELRO -DEFAULT_BIND_NOW"

	if hardened_gcc_works ; then
		einfo "Updating gcc to use automatic PIE + SSP building ..."
		gcc_hard_flags+=" -DEFAULT_PIE_SSP"
	elif hardened_gcc_works pie ; then
		einfo "Updating gcc to use automatic PIE building ..."
		ewarn "SSP has not been enabled by default"
		gcc_hard_flags+=" -DEFAULT_PIE"
	elif hardened_gcc_works ssp ; then
		einfo "Updating gcc to use automatic SSP building ..."
		ewarn "PIE has not been enabled by default"
		gcc_hard_flags+=" -DEFAULT_SSP"
	else
		# do nothing if hardened isnt supported, but dont die either
		ewarn "hardened is not supported for this arch in this gcc version"
		ebeep
		return 0
	fi

	sed -i \
		-e "/^HARD_CFLAGS = /s|=|= ${gcc_hard_flags} |" \
		"${S}"/gcc/Makefile.in || die

	# rebrand to make bug reports easier
	BRANDING_GCC_PKGVERSION=${BRANDING_GCC_PKGVERSION/Gentoo/Gentoo Hardened}
}

create_gcc_env_entry() {
	dodir /etc/env.d/gcc
	local gcc_envd_base="/etc/env.d/gcc/${CTARGET}-${GCC_CONFIG_VER}"

	local gcc_specs_file
	local gcc_envd_file="${D}${gcc_envd_base}"
	if [[ -z $1 ]] ; then
		# I'm leaving the following commented out to remind me that it
		# was an insanely -bad- idea. Stuff broke. GCC_SPECS isnt unset
		# on chroot or in non-toolchain.eclass gcc ebuilds!
		#gcc_specs_file="${LIBPATH}/specs"
		gcc_specs_file=""
	else
		gcc_envd_file+="-$1"
		gcc_specs_file="${LIBPATH}/$1.specs"
	fi

	# We want to list the default ABI's LIBPATH first so libtool
	# searches that directory first.  This is a temporary
	# workaround for libtool being stupid and using .la's from
	# conflicting ABIs by using the first one in the search path
	local ldpaths mosdirs
	if tc_version_is_at_least 3.2 ; then
		local mdir mosdir abi ldpath
		for abi in $(get_all_abis TARGET) ; do
			mdir=$($(XGCC) $(get_abi_CFLAGS ${abi}) --print-multi-directory)
			ldpath=${LIBPATH}
			[[ ${mdir} != "." ]] && ldpath+="/${mdir}"
			ldpaths="${ldpath}${ldpaths:+:${ldpaths}}"

			mosdir=$($(XGCC) $(get_abi_CFLAGS ${abi}) -print-multi-os-directory)
			mosdirs="${mosdir}${mosdirs:+:${mosdirs}}"
		done
	else
		# Older gcc's didn't do multilib, so logic is simple.
		ldpaths=${LIBPATH}
	fi

	cat <<-EOF > ${gcc_envd_file}
	PATH="${BINPATH}"
	ROOTPATH="${BINPATH}"
	GCC_PATH="${BINPATH}"
	LDPATH="${ldpaths}"
	MANPATH="${DATAPATH}/man"
	INFOPATH="${DATAPATH}/info"
	STDCXX_INCDIR="${STDCXX_INCDIR##*/}"
	CTARGET="${CTARGET}"
	GCC_SPECS="${gcc_specs_file}"
	MULTIOSDIRS="${mosdirs}"
	EOF
}
setup_minispecs_gcc_build_specs() {
	# Setup the "build.specs" file for gcc 4.3 to use when building.
	if hardened_gcc_works pie ; then
		cat "${WORKDIR}"/specs/pie.specs >> "${WORKDIR}"/build.specs
	fi
	if hardened_gcc_works ssp ; then
		for s in ssp sspall ; do
			cat "${WORKDIR}"/specs/${s}.specs >> "${WORKDIR}"/build.specs
		done
	fi
	for s in nostrict znow ; do
		cat "${WORKDIR}"/specs/${s}.specs >> "${WORKDIR}"/build.specs
	done
	export GCC_SPECS="${WORKDIR}"/build.specs
}
copy_minispecs_gcc_specs() {
	# setup the hardenedno* specs files and the vanilla specs file.
	if hardened_gcc_works ; then
		create_gcc_env_entry hardenednopiessp
	fi
	if hardened_gcc_works pie ; then
		create_gcc_env_entry hardenednopie
	fi
	if hardened_gcc_works ssp ; then
		create_gcc_env_entry hardenednossp
	fi
	create_gcc_env_entry vanilla
	insinto ${LIBPATH}
	doins "${WORKDIR}"/specs/*.specs || die "failed to install specs"
	# Build system specs file which, if it exists, must be a complete set of
	# specs as it completely and unconditionally overrides the builtin specs.
	# For gcc 4.3
	if ! tc_version_is_at_least 4.4 ; then
		$(XGCC) -dumpspecs > "${WORKDIR}"/specs/specs
		cat "${WORKDIR}"/build.specs >> "${WORKDIR}"/specs/specs
		doins "${WORKDIR}"/specs/specs || die "failed to install the specs file"
	fi
}

#----<< specs + env.d logic >>----

#---->> pkg_* <<----
toolchain_pkg_setup() {
	if [[ -n ${PRERELEASE}${SNAPSHOT} || ${PV} == *9999* ]] &&
	   [[ -z ${I_PROMISE_TO_SUPPLY_PATCHES_WITH_BUGS} ]]
	then
		die "Please \`export I_PROMISE_TO_SUPPLY_PATCHES_WITH_BUGS=1\` or define it in your make.conf if you want to use this version." \
			"This is to try and cut down on people filing bugs for a compiler we do not currently support."
	fi

	# we dont want to use the installed compiler's specs to build gcc!
	unset GCC_SPECS

	if ! use_if_iuse cxx ; then
		use_if_iuse go && ewarn 'Go requires a C++ compiler, disabled due to USE="-cxx"'
		use_if_iuse objc++ && ewarn 'Obj-C++ requires a C++ compiler, disabled due to USE="-cxx"'
		use_if_iuse gcj && ewarn 'GCJ requires a C++ compiler, disabled due to USE="-cxx"'
	fi

	want_minispecs

	unset LANGUAGES #265283
}

toolchain_pkg_postinst() {
	do_gcc_config

	if ! is_crosscompile ; then
		echo
		ewarn "If you have issues with packages unable to locate libstdc++.la,"
		ewarn "then try running 'fix_libtool_files.sh' on the old gcc versions."
		echo
		ewarn "You might want to review the GCC upgrade guide when moving between"
		ewarn "major versions (like 4.2 to 4.3):"
		ewarn "http://www.gentoo.org/doc/en/gcc-upgrading.xml"
		echo
	fi

	if ! is_crosscompile ; then
		# hack to prevent collisions between SLOTs

		# Clean up old paths
		rm -f "${ROOT}"/*/rcscripts/awk/fixlafiles.awk "${ROOT}"/sbin/fix_libtool_files.sh
		rmdir "${ROOT}"/*/rcscripts{/awk,} 2>/dev/null

		mkdir -p "${ROOT}"/usr/{share/gcc-data,sbin,bin}
		cp "${ROOT}/${DATAPATH}"/fixlafiles.awk "${ROOT}"/usr/share/gcc-data/ || die
		cp "${ROOT}/${DATAPATH}"/fix_libtool_files.sh "${ROOT}"/usr/sbin/ || die

		# Since these aren't critical files and portage sucks with
		# handling of binpkgs, don't require these to be found
		cp "${ROOT}/${DATAPATH}"/c{89,99} "${ROOT}"/usr/bin/ 2>/dev/null
	fi
}

toolchain_pkg_postrm() {
	# to make our lives easier (and saner), we do the fix_libtool stuff here.
	# rather than checking SLOT's and trying in upgrade paths, we just see if
	# the common libstdc++.la exists in the ${LIBPATH} of the gcc that we are
	# unmerging.  if it does, that means this was a simple re-emerge.

	# clean up the cruft left behind by cross-compilers
	if is_crosscompile ; then
		if [[ -z $(ls "${ROOT}"/etc/env.d/gcc/${CTARGET}* 2>/dev/null) ]] ; then
			rm -f "${ROOT}"/etc/env.d/gcc/config-${CTARGET}
			rm -f "${ROOT}"/etc/env.d/??gcc-${CTARGET}
			rm -f "${ROOT}"/usr/bin/${CTARGET}-{gcc,{g,c}++}{,32,64}
		fi
		return 0
	fi

	# ROOT isnt handled by the script
	[[ ${ROOT} != "/" ]] && return 0

	if [[ ! -e ${LIBPATH}/libstdc++.so ]] ; then
		# make sure the profile is sane during same-slot upgrade #289403
		do_gcc_config

		einfo "Running 'fix_libtool_files.sh ${GCC_RELEASE_VER}'"
		/usr/sbin/fix_libtool_files.sh ${GCC_RELEASE_VER}
		if [[ -n ${BRANCH_UPDATE} ]] ; then
			einfo "Running 'fix_libtool_files.sh ${GCC_RELEASE_VER}-${BRANCH_UPDATE}'"
			/usr/sbin/fix_libtool_files.sh ${GCC_RELEASE_VER}-${BRANCH_UPDATE}
		fi
	fi

	return 0
}

#---->> pkg_* <<----

#---->> src_* <<----

guess_patch_type_in_dir() {
	[[ -n $(ls "$1"/*.bz2 2>/dev/null) ]] \
		&& EPATCH_SUFFIX="patch.bz2" \
		|| EPATCH_SUFFIX="patch"
}
do_gcc_rename_java_bins() {
	# bug #139918 - conflict between gcc and java-config-2 for ownership of
	# /usr/bin/rmi{c,registry}.	 Done with mv & sed rather than a patch
	# because patches would be large (thanks to the rename of man files),
	# and it's clear from the sed invocations that all that changes is the
	# rmi{c,registry} names to grmi{c,registry} names.
	# Kevin F. Quinn 2006-07-12
	einfo "Renaming jdk executables rmic and rmiregistry to grmic and grmiregistry."
	# 1) Move the man files if present (missing prior to gcc-3.4)
	for manfile in rmic rmiregistry; do
		[[ -f ${S}/gcc/doc/${manfile}.1 ]] || continue
		mv "${S}"/gcc/doc/${manfile}.1 "${S}"/gcc/doc/g${manfile}.1
	done
	# 2) Fixup references in the docs if present (mission prior to gcc-3.4)
	for jfile in gcc/doc/gcj.info gcc/doc/grmic.1 gcc/doc/grmiregistry.1 gcc/java/gcj.texi; do
		[[ -f ${S}/${jfile} ]] || continue
		sed -i -e 's:rmiregistry:grmiregistry:g' "${S}"/${jfile} ||
			die "Failed to fixup file ${jfile} for rename to grmiregistry"
		sed -i -e 's:rmic:grmic:g' "${S}"/${jfile} ||
			die "Failed to fixup file ${jfile} for rename to grmic"
	done
	# 3) Fixup Makefiles to build the changed executable names
	#	 These are present in all 3.x versions, and are the important bit
	#	 to get gcc to build with the new names.
	for jfile in libjava/Makefile.am libjava/Makefile.in gcc/java/Make-lang.in; do
		sed -i -e 's:rmiregistry:grmiregistry:g' "${S}"/${jfile} ||
			die "Failed to fixup file ${jfile} for rename to grmiregistry"
		# Careful with rmic on these files; it's also the name of a directory
		# which should be left unchanged.  Replace occurrences of 'rmic$',
		# 'rmic_' and 'rmic '.
		sed -i -e 's:rmic\([$_ ]\):grmic\1:g' "${S}"/${jfile} ||
			die "Failed to fixup file ${jfile} for rename to grmic"
	done
}
toolchain_src_unpack() {
	[[ -z ${UCLIBC_VER} ]] && [[ ${CTARGET} == *-uclibc* ]] && die "Sorry, this version does not support uClibc"

	if [[ ${PV} == *9999* ]]; then
		git-2_src_unpack
	else
		gcc_quick_unpack
	fi

	export BRANDING_GCC_PKGVERSION="Gentoo ${GCC_PVR}"
	cd "${S}"

	if ! use vanilla ; then
		if [[ -n ${PATCH_VER} ]] ; then
			guess_patch_type_in_dir "${WORKDIR}"/patch
			EPATCH_MULTI_MSG="Applying Gentoo patches ..." \
			epatch "${WORKDIR}"/patch
			BRANDING_GCC_PKGVERSION="${BRANDING_GCC_PKGVERSION} p${PATCH_VER}"
		fi
		if [[ -n ${UCLIBC_VER} ]] ; then
			guess_patch_type_in_dir "${WORKDIR}"/uclibc
			EPATCH_MULTI_MSG="Applying uClibc patches ..." \
			epatch "${WORKDIR}"/uclibc
		fi
	fi
	do_gcc_HTB_patches
	do_gcc_PIE_patches
	epatch_user

	use hardened && make_gcc_hard

	# install the libstdc++ python into the right location
	# http://gcc.gnu.org/PR51368
	if tc_version_is_at_least 4.5 && ! tc_version_is_at_least 4.7 ; then
		sed -i \
			'/^pythondir =/s:=.*:= $(datadir)/python:' \
			"${S}"/libstdc++-v3/python/Makefile.in || die
	fi

	# make sure the pkg config files install into multilib dirs.
	# since we configure with just one --libdir, we can't use that
	# (as gcc itself takes care of building multilibs).  #435728
	find "${S}" -name Makefile.in \
		-exec sed -i '/^pkgconfigdir/s:=.*:=$(toolexeclibdir)/pkgconfig:' {} +

	# No idea when this first started being fixed, but let's go with 4.3.x for now
	if ! tc_version_is_at_least 4.3 ; then
		fix_files=""
		for x in contrib/test_summary libstdc++-v3/scripts/check_survey.in ; do
			[[ -e ${x} ]] && fix_files="${fix_files} ${x}"
		done
		ht_fix_file ${fix_files} */configure *.sh */Makefile.in
	fi

	setup_multilib_osdirnames

	gcc_version_patch
	if tc_version_is_at_least 4.1 ; then
		if [[ -n ${SNAPSHOT} || -n ${PRERELEASE} ]] ; then
			# BASE-VER must be a three-digit version number
			# followed by an optional -pre string
			#   eg. 4.5.1, 4.6.2-pre20120213, 4.7.0-pre9999
			# If BASE-VER differs from ${PV/_/-} then libraries get installed in
			# the wrong directory.
			echo ${PV/_/-} > "${S}"/gcc/BASE-VER
		fi
	fi

	# >= gcc-4.3 doesn't bundle ecj.jar, so copy it
	if tc_version_is_at_least 4.3 && use gcj ; then
		if tc_version_is_at_least "4.5" ; then
			einfo "Copying ecj-4.5.jar"
			cp -pPR "${DISTDIR}/ecj-4.5.jar" "${S}/ecj.jar" || die
		elif tc_version_is_at_least "4.3" ; then
			einfo "Copying ecj-4.3.jar"
			cp -pPR "${DISTDIR}/ecj-4.3.jar" "${S}/ecj.jar" || die
		fi
	fi

	# disable --as-needed from being compiled into gcc specs
	# natively when using a gcc version < 3.4.4
	# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=14992
	if ! tc_version_is_at_least 3.4.4 ; then
		sed -i -e s/HAVE_LD_AS_NEEDED/USE_LD_AS_NEEDED/g "${S}"/gcc/config.in
	fi

	# In gcc 3.3.x and 3.4.x, rename the java bins to gcc-specific names
	# in line with gcc-4.
	if tc_version_is_at_least 3.3 && ! tc_version_is_at_least 4.0 ; then
		do_gcc_rename_java_bins
	fi

	# Prevent libffi from being installed
	if tc_version_is_at_least 3.0 ; then
		sed -i -e 's/\(install.*:\) install-.*recursive/\1/' "${S}"/libffi/Makefile.in || die
		sed -i -e 's/\(install-data-am:\).*/\1/' "${S}"/libffi/include/Makefile.in || die
	fi

	# Fixup libtool to correctly generate .la files with portage
	cd "${S}"
	elibtoolize --portage --shallow --no-uclibc

	gnuconfig_update

	# update configure files
	local f
	einfo "Fixing misc issues in configure files"
	tc_version_is_at_least 4.1 && epatch "${GCC_FILESDIR}"/gcc-configure-texinfo.patch
	for f in $(grep -l 'autoconf version 2.13' $(find "${S}" -name configure)) ; do
		ebegin "  Updating ${f/${S}\/} [LANG]"
		patch "${f}" "${GCC_FILESDIR}"/gcc-configure-LANG.patch >& "${T}"/configure-patch.log \
			|| eerror "Please file a bug about this"
		eend $?
	done
	sed -i 's|A-Za-z0-9|[:alnum:]|g' "${S}"/gcc/*.awk #215828

	if [[ -x contrib/gcc_update ]] ; then
		einfo "Touching generated files"
		./contrib/gcc_update --touch | \
			while read f ; do
				einfo "  ${f%%...}"
			done
	fi
}

gcc-abi-map() {
	# Convert the ABI name we use in Gentoo to what gcc uses
	local map=()
	case ${CTARGET} in
	mips*)   map=("o32 32" "n32 n32" "n64 64") ;;
	x86_64*) map=("amd64 m64" "x86 m32" "x32 mx32") ;;
	esac

	local m
	for m in "${map[@]}" ; do
		l=( ${m} )
		[[ $1 == ${l[0]} ]] && echo ${l[1]} && break
	done
}

gcc-multilib-configure() {
	if ! is_multilib ; then
		confgcc+=" --disable-multilib"
		# Fun times: if we are building for a target that has multiple
		# possible ABI formats, and the user has told us to pick one
		# that isn't the default, then not specifying it via the list
		# below will break that on us.
	else
		confgcc+=" --enable-multilib"
	fi

	# translate our notion of multilibs into gcc's
	local abi list
	for abi in $(get_all_abis TARGET) ; do
		local l=$(gcc-abi-map ${abi})
		[[ -n ${l} ]] && list+=",${l}"
	done
	if [[ -n ${list} ]] ; then
		case ${CTARGET} in
		x86_64*)
			tc_version_is_at_least 4.7 && confgcc+=" --with-multilib-list=${list:1}"
			;;
		esac
	fi
}

gcc-compiler-configure() {
	gcc-multilib-configure

	if tc_version_is_at_least "4.0" ; then
		if in_iuse mudflap ; then
			confgcc+=" $(use_enable mudflap libmudflap)"
		else
			confgcc+=" --disable-libmudflap"
		fi

		if use_if_iuse libssp ; then
			confgcc+=" --enable-libssp"
		else
			export gcc_cv_libc_provides_ssp=yes
			confgcc+=" --disable-libssp"
		fi

		# If we want hardened support with the newer piepatchset for >=gcc 4.4
		if tc_version_is_at_least 4.4 && want_minispecs ; then
			confgcc+=" $(use_enable hardened esp)"
		fi

		if tc_version_is_at_least "4.2" ; then
			if in_iuse openmp ; then
				# Make sure target has pthreads support. #326757 #335883
				# There shouldn't be a chicken&egg problem here as openmp won't
				# build without a C library, and you can't build that w/out
				# already having a compiler ...
				if ! is_crosscompile || \
				   $(tc-getCPP ${CTARGET}) -E - <<<"#include <pthread.h>" >& /dev/null
				then
					confgcc+=" $(use_enable openmp libgomp)"
				else
					# Force disable as the configure script can be dumb #359855
					confgcc+=" --disable-libgomp"
				fi
			else
				# For gcc variants where we don't want openmp (e.g. kgcc)
				confgcc+=" --disable-libgomp"
			fi
		fi

		# Stick the python scripts in their own slotted directory
		# bug #279252
		#
		#  --with-python-dir=DIR
		#  Specifies where to install the Python modules used for aot-compile. DIR
		#  should not include the prefix used in installation. For example, if the
		#  Python modules are to be installed in /usr/lib/python2.5/site-packages,
		#  then --with-python-dir=/lib/python2.5/site-packages should be passed.
		#
		# This should translate into "/share/gcc-data/${CTARGET}/${GCC_CONFIG_VER}/python"
		if tc_version_is_at_least "4.4" ; then
			confgcc+=" --with-python-dir=${DATAPATH/$PREFIX/}/python"
		fi
	fi

	# Enable build warnings by default with cross-compilers when system
	# paths are included (e.g. via -I flags).
	is_crosscompile && confgcc+=" --enable-poison-system-directories"

	# For newer versions of gcc, use the default ("release"), because no
	# one (even upstream apparently) tests with it disabled. #317217
	if tc_version_is_at_least 4 || [[ -n ${GCC_CHECKS_LIST} ]] ; then
		confgcc+=" --enable-checking=${GCC_CHECKS_LIST:-release}"
	else
		confgcc+=" --disable-checking"
	fi

	# GTK+ is preferred over xlib in 3.4.x (xlib is unmaintained
	# right now). Much thanks to <csm@gnu.org> for the heads up.
	# Travis Tilley <lv@gentoo.org>	 (11 Jul 2004)
	if ! is_gcj ; then
		confgcc+=" --disable-libgcj"
	elif use gtk ; then
		confgcc+=" --enable-java-awt=gtk"
	fi

	# allow gcc to search for clock funcs in the main C lib.
	# if it can't find them, then tough cookies -- we aren't
	# going to link in -lrt to all C++ apps.  #411681
	if tc_version_is_at_least 4.4 && is_cxx ; then
		confgcc+=" --enable-libstdcxx-time"
	fi

	# newer gcc versions like to bootstrap themselves with C++,
	# so we need to manually disable it ourselves
	if tc_version_is_at_least 4.7 && ! is_cxx ; then
		confgcc+=" --disable-build-with-cxx --disable-build-poststage1-with-cxx"
	fi

	# newer gcc's come with libquadmath, but only fortran uses
	# it, so auto punt it when we don't care
	if tc_version_is_at_least 4.6 && ! is_fortran ; then
		confgcc+=" --disable-libquadmath"
	fi

	local with_abi_map=()
	case $(tc-arch) in
		arm)	#264534 #414395
			local a arm_arch=${CTARGET%%-*}
			# Remove trailing endian variations first: eb el be bl b l
			for a in e{b,l} {b,l}e b l ; do
				if [[ ${arm_arch} == *${a} ]] ; then
					arm_arch=${arm_arch%${a}}
					break
				fi
			done
			# Convert armv7{a,r,m} to armv7-{a,r,m}
			[[ ${arm_arch} == armv7? ]] && arm_arch=${arm_arch/7/7-}
			# See if this is a valid --with-arch flag
			if (srcdir=${S}/gcc target=${CTARGET} with_arch=${arm_arch};
			    . "${srcdir}"/config.gcc) &>/dev/null
			then
				confgcc+=" --with-arch=${arm_arch}"
			fi

			# Enable hardvfp
			if [[ $(tc-is-softfloat) == "no" ]] && \
			   [[ ${CTARGET} == armv[67]* ]] && \
			   tc_version_is_at_least "4.5"
			then
				# Follow the new arm hardfp distro standard by default
				confgcc+=" --with-float=hard"
				case ${CTARGET} in
				armv6*) confgcc+=" --with-fpu=vfp" ;;
				armv7*) confgcc+=" --with-fpu=vfpv3-d16" ;;
				esac
			fi
			;;
		# Add --with-abi flags to set default ABI
		mips)
			confgcc+=" --with-abi=$(gcc-abi-map ${TARGET_DEFAULT_ABI})"
			;;
		amd64)
			# drop the older/ABI checks once this get's merged into some
			# version of gcc upstream
			if tc_version_is_at_least 4.7 && has x32 $(get_all_abis TARGET) ; then
				confgcc+=" --with-abi=$(gcc-abi-map ${TARGET_DEFAULT_ABI})"
			fi
			;;
		# Default arch for x86 is normally i386, lets give it a bump
		# since glibc will do so based on CTARGET anyways
		x86)
			confgcc+=" --with-arch=${CTARGET%%-*}"
			;;
		# Enable sjlj exceptions for backward compatibility on hppa
		hppa)
			[[ ${GCCMAJOR} == "3" ]] && confgcc+=" --enable-sjlj-exceptions"
			;;
		# Set up defaults based on current CFLAGS
		ppc)
			is-flagq -mfloat-gprs=double && confgcc+=" --enable-e500-double"
			[[ ${CTARGET//_/-} == *-e500v2-* ]] && confgcc+=" --enable-e500-double"
			;;
	esac

	local GCC_LANG="c"
	is_cxx && GCC_LANG+=",c++"
	is_d   && GCC_LANG+=",d"
	is_gcj && GCC_LANG+=",java"
	is_go  && GCC_LANG+=",go"
	if is_objc || is_objcxx ; then
		GCC_LANG+=",objc"
		if tc_version_is_at_least "4.0" ; then
			use objc-gc && confgcc+=" --enable-objc-gc"
		fi
		is_objcxx && GCC_LANG+=",obj-c++"
	fi
	is_treelang && GCC_LANG+=",treelang"

	# fortran support just got sillier! the lang value can be f77 for
	# fortran77, f95 for fortran95, or just plain old fortran for the
	# currently supported standard depending on gcc version.
	is_fortran && GCC_LANG+=",fortran"
	is_f77 && GCC_LANG+=",f77"
	is_f95 && GCC_LANG+=",f95"

	# We do NOT want 'ADA support' in here!
	# is_ada && GCC_LANG+=",ada"

	einfo "configuring for GCC_LANG: ${GCC_LANG}"
	confgcc+=" --enable-languages=${GCC_LANG}"
}

gcc_do_configure() {
	local confgcc

	# Set configuration based on path variables
	confgcc+=" \
		--prefix=${PREFIX} \
		--bindir=${BINPATH} \
		--includedir=${INCLUDEPATH} \
		--datadir=${DATAPATH} \
		--mandir=${DATAPATH}/man \
		--infodir=${DATAPATH}/info \
		--with-gxx-include-dir=${STDCXX_INCDIR}"
	# On Darwin we need libdir to be set in order to get correct install names
	# for things like libobjc-gnu, libgcj and libfortran.  If we enable it on
	# non-Darwin we screw up the behaviour this eclass relies on.  We in
	# particular need this over --libdir for bug #255315.
	[[ ${CTARGET} == *-darwin* ]] && \
		confgcc+=" --enable-version-specific-runtime-libs"

	# All our cross-compile logic goes here !  woo !
	confgcc+=" --host=${CHOST}"
	if is_crosscompile || tc-is-cross-compiler ; then
		# Straight from the GCC install doc:
		# "GCC has code to correctly determine the correct value for target
		# for nearly all native systems. Therefore, we highly recommend you
		# not provide a configure target when configuring a native compiler."
		confgcc+=" --target=${CTARGET}"
	fi
	[[ -n ${CBUILD} ]] && confgcc+=" --build=${CBUILD}"

	# ppc altivec support
	confgcc+=" $(use_enable altivec)"

	# gcc has fixed-point arithmetic support in 4.3 for mips targets that can
	# significantly increase compile time by several hours.  This will allow
	# users to control this feature in the event they need the support.
	tc_version_is_at_least "4.3" && confgcc+=" $(use_enable fixed-point)"

	# Graphite support was added in 4.4, which depends on external libraries
	# for optimizations.  Current versions use cloog-ppl (cloog fork with Parma
	# PPL backend).  Sometime in the future we will use upstream cloog with the
	# ISL backend (note: PPL will still be a requirement).  cloog-ppl's include
	# path was modified to prevent collisions between the two packages (library
	# names are different).
	#
	# We disable the PPL version check so we can use >=ppl-0.11.
	if tc_version_is_at_least "4.4"; then
		confgcc+=" $(use_with graphite ppl)"
		confgcc+=" $(use_with graphite cloog)"
		if use graphite; then
			confgcc+=" --disable-ppl-version-check"
			confgcc+=" --with-cloog-include=/usr/include/cloog-ppl"
		fi
	fi

	# LTO support was added in 4.5, which depends upon elfutils.  This allows
	# users to enable that option, and pull in the additional library.  In 4.6,
	# the dependency is no longer required.
	if tc_version_is_at_least "4.6" ; then
		confgcc+=" --enable-lto"
	elif tc_version_is_at_least "4.5" ; then
		confgcc+=" $(use_enable lto)"
	fi

	case $(tc-is-softfloat) in
	yes)    confgcc+=" --with-float=soft" ;;
	softfp) confgcc+=" --with-float=softfp" ;;
	*)
		# If they've explicitly opt-ed in, do hardfloat,
		# otherwise let the gcc default kick in.
		[[ ${CTARGET//_/-} == *-hardfloat-* ]] \
			&& confgcc+=" --with-float=hard"
		;;
	esac

	# Native Language Support
	if use nls ; then
		confgcc+=" --enable-nls --without-included-gettext"
	else
		confgcc+=" --disable-nls"
	fi

	# reasonably sane globals (hopefully)
	confgcc+=" \
		--with-system-zlib \
		--enable-obsolete \
		--disable-werror \
		--enable-secureplt"

	gcc-compiler-configure || die

	if is_crosscompile ; then
		# When building a stage1 cross-compiler (just C compiler), we have to
		# disable a bunch of features or gcc goes boom
		local needed_libc=""
		case ${CTARGET} in
			*-linux)		 needed_libc=no-fucking-clue;;
			*-dietlibc)		 needed_libc=dietlibc;;
			*-elf|*-eabi)	 needed_libc=newlib;;
			*-freebsd*)		 needed_libc=freebsd-lib;;
			*-gnu*)			 needed_libc=glibc;;
			*-klibc)		 needed_libc=klibc;;
			*-uclibc*)
				if ! echo '#include <features.h>' | \
				   $(tc-getCPP ${CTARGET}) -E -dD - 2>/dev/null | \
				   grep -q __HAVE_SHARED__
				then #291870
					confgcc+=" --disable-shared"
				fi
				needed_libc=uclibc
				;;
			*-cygwin)		 needed_libc=cygwin;;
			x86_64-*-mingw*|\
			*-w64-mingw*)	 needed_libc=mingw64-runtime;;
			mingw*|*-mingw*) needed_libc=mingw-runtime;;
			avr)			 confgcc+=" --enable-shared --disable-threads";;
		esac
		if [[ -n ${needed_libc} ]] ; then
			if ! has_version ${CATEGORY}/${needed_libc} ; then
				confgcc+=" --disable-shared --disable-threads --without-headers"
			elif built_with_use --hidden --missing false ${CATEGORY}/${needed_libc} crosscompile_opts_headers-only ; then
				confgcc+=" --disable-shared --with-sysroot=${PREFIX}/${CTARGET}"
			else
				confgcc+=" --with-sysroot=${PREFIX}/${CTARGET}"
			fi
		fi

		tc_version_is_at_least 4.2 && confgcc+=" --disable-bootstrap"
	else
		if tc-is-static-only ; then
			confgcc+=" --disable-shared"
		else
			confgcc+=" --enable-shared"
		fi
		case ${CHOST} in
			mingw*|*-mingw*|*-cygwin)
				confgcc+=" --enable-threads=win32" ;;
			*)
				confgcc+=" --enable-threads=posix" ;;
		esac
	fi
	# __cxa_atexit is "essential for fully standards-compliant handling of
	# destructors", but apparently requires glibc.
	case ${CTARGET} in
	*-uclibc*)
		confgcc+=" --disable-__cxa_atexit --enable-target-optspace $(use_enable nptl tls)"
		[[ ${GCCMAJOR}.${GCCMINOR} == 3.3 ]] && confgcc+=" --enable-sjlj-exceptions"
		if tc_version_is_at_least 3.4 && ! tc_version_is_at_least 4.3 ; then
			confgcc+=" --enable-clocale=uclibc"
		fi
		;;
	*-elf|*-eabi)
		confgcc+=" --with-newlib"
		;;
	*-gnu*)
		confgcc+=" --enable-__cxa_atexit"
		confgcc+=" --enable-clocale=gnu"
		;;
	*-freebsd*)
		confgcc+=" --enable-__cxa_atexit"
		;;
	*-solaris*)
		confgcc+=" --enable-__cxa_atexit"
		;;
	esac
	tc_version_is_at_least 3.4 || confgcc+=" --disable-libunwind-exceptions"

	# if the target can do biarch (-m32/-m64), enable it.  overhead should
	# be small, and should simplify building of 64bit kernels in a 32bit
	# userland by not needing sys-devel/kgcc64.  #349405
	case $(tc-arch) in
	ppc|ppc64) tc_version_is_at_least 3.4 && confgcc+=" --enable-targets=all" ;;
	sparc)     tc_version_is_at_least 4.4 && confgcc+=" --enable-targets=all" ;;
	amd64|x86) tc_version_is_at_least 4.3 && confgcc+=" --enable-targets=all" ;;
	esac

	tc_version_is_at_least 4.3 && set -- "$@" \
		--with-bugurl=http://bugs.gentoo.org/ \
		--with-pkgversion="${BRANDING_GCC_PKGVERSION}"
	set -- ${confgcc} "$@" ${EXTRA_ECONF}

	# Do not let the X detection get in our way.  We know things can be found
	# via system paths, so no need to hardcode things that'll break multilib.
	# Older gcc versions will detect ac_x_libraries=/usr/lib64 which ends up
	# killing the 32bit builds which want /usr/lib.
	export ac_cv_have_x='have_x=yes ac_x_includes= ac_x_libraries='

	# Nothing wrong with a good dose of verbosity
	echo
	einfo "PREFIX:			${PREFIX}"
	einfo "BINPATH:			${BINPATH}"
	einfo "LIBPATH:			${LIBPATH}"
	einfo "DATAPATH:		${DATAPATH}"
	einfo "STDCXX_INCDIR:	${STDCXX_INCDIR}"
	echo
	einfo "Configuring GCC with: ${@//--/\n\t--}"
	echo

	# Build in a separate build tree
	mkdir -p "${WORKDIR}"/build
	pushd "${WORKDIR}"/build > /dev/null

	# and now to do the actual configuration
	addwrite /dev/zero
	echo "${S}"/configure "$@"
	"${S}"/configure "$@" || die "failed to run configure"

	# return to whatever directory we were in before
	popd > /dev/null
}

has toolchain_death_notice ${EBUILD_DEATH_HOOKS} || EBUILD_DEATH_HOOKS+=" toolchain_death_notice"
toolchain_death_notice() {
	pushd "${WORKDIR}"/build >/dev/null
	tar jcf gcc-build-logs.tar.bz2 $(find -name config.log)
	eerror
	eerror "Please include ${PWD}/gcc-build-logs.tar.bz2 in your bug report"
	eerror
	popd >/dev/null
}

# This function accepts one optional argument, the make target to be used.
# If ommitted, gcc_do_make will try to guess whether it should use all,
# profiledbootstrap, or bootstrap-lean depending on CTARGET and arch. An
# example of how to use this function:
#
#	gcc_do_make all-target-libstdc++-v3
#
# In addition to the target to be used, the following variables alter the
# behavior of this function:
#
#	LDFLAGS
#			Flags to pass to ld
#
#	STAGE1_CFLAGS
#			CFLAGS to use during stage1 of a gcc bootstrap
#
#	BOOT_CFLAGS
#			CFLAGS to use during stages 2+3 of a gcc bootstrap.
#
# Travis Tilley <lv@gentoo.org> (04 Sep 2004)
#
gcc_do_make() {
	# Fix for libtool-portage.patch
	local OLDS=${S}
	S=${WORKDIR}/build

	# Set make target to $1 if passed
	[[ -n $1 ]] && GCC_MAKE_TARGET=$1
	# default target
	if is_crosscompile || tc-is-cross-compiler ; then
		# 3 stage bootstrapping doesnt quite work when you cant run the
		# resulting binaries natively ^^;
		GCC_MAKE_TARGET=${GCC_MAKE_TARGET-all}
	else
		GCC_MAKE_TARGET=${GCC_MAKE_TARGET-bootstrap-lean}
	fi

	# the gcc docs state that parallel make isnt supported for the
	# profiledbootstrap target, as collisions in profile collecting may occur.
	# boundschecking also seems to introduce parallel build issues.
	if [[ ${GCC_MAKE_TARGET} == "profiledbootstrap" ]] ||
	   use_if_iuse boundschecking
	then
		export MAKEOPTS="${MAKEOPTS} -j1"
	fi

	if [[ ${GCC_MAKE_TARGET} == "all" ]] ; then
		STAGE1_CFLAGS=${STAGE1_CFLAGS-"${CFLAGS}"}
	elif [[ $(gcc-version) == "3.4" && ${GCC_BRANCH_VER} == "3.4" ]] && gcc-specs-ssp ; then
		# See bug #79852
		STAGE1_CFLAGS=${STAGE1_CFLAGS-"-O2"}
	fi

	if is_crosscompile; then
		# In 3.4, BOOT_CFLAGS is never used on a crosscompile...
		# but I'll leave this in anyways as someone might have had
		# some reason for putting it in here... --eradicator
		BOOT_CFLAGS=${BOOT_CFLAGS-"-O2"}
	else
		# we only want to use the system's CFLAGS if not building a
		# cross-compiler.
		BOOT_CFLAGS=${BOOT_CFLAGS-"$(get_abi_CFLAGS ${TARGET_DEFAULT_ABI}) ${CFLAGS}"}
	fi

	pushd "${WORKDIR}"/build

	emake \
		LDFLAGS="${LDFLAGS}" \
		STAGE1_CFLAGS="${STAGE1_CFLAGS}" \
		LIBPATH="${LIBPATH}" \
		BOOT_CFLAGS="${BOOT_CFLAGS}" \
		${GCC_MAKE_TARGET} \
		|| die "emake failed with ${GCC_MAKE_TARGET}"

	if ! is_crosscompile && use cxx && use_if_iuse doc ; then
		if type -p doxygen > /dev/null ; then
			if tc_version_is_at_least 4.3 ; then
				cd "${CTARGET}"/libstdc++-v3/doc
				emake doc-man-doxygen || ewarn "failed to make docs"
			elif tc_version_is_at_least 3.0 ; then
				cd "${CTARGET}"/libstdc++-v3
				emake doxygen-man || ewarn "failed to make docs"
			fi
		else
			ewarn "Skipping libstdc++ manpage generation since you don't have doxygen installed"
		fi
	fi

	popd
}

# This is mostly a stub function to be overwritten in an ebuild
gcc_do_filter_flags() {
	strip-flags

	# In general gcc does not like optimization, and add -O2 where
	# it is safe.  This is especially true for gcc 3.3 + 3.4
	replace-flags -O? -O2

	# ... sure, why not?
	strip-unsupported-flags

	# dont want to funk ourselves
	filter-flags '-mabi*' -m31 -m32 -m64

	case ${GCC_BRANCH_VER} in
	3.2|3.3)
		replace-cpu-flags k8 athlon64 opteron i686 x86-64
		replace-cpu-flags pentium-m pentium3m pentium3
		case $(tc-arch) in
			amd64|x86) filter-flags '-mtune=*' ;;
			# in gcc 3.3 there is a bug on ppc64 where if -mcpu is used,
			# the compiler wrongly assumes a 32bit target
			ppc64) filter-flags "-mcpu=*";;
		esac
		case $(tc-arch) in
			amd64) replace-cpu-flags core2 nocona;;
			x86)   replace-cpu-flags core2 prescott;;
		esac

		replace-cpu-flags G3 750
		replace-cpu-flags G4 7400
		replace-cpu-flags G5 7400

		# XXX: should add a sed or something to query all supported flags
		#      from the gcc source and trim everything else ...
		filter-flags -f{no-,}unit-at-a-time -f{no-,}web -mno-tls-direct-seg-refs
		filter-flags -f{no-,}stack-protector{,-all}
		filter-flags -fvisibility-inlines-hidden -fvisibility=hidden
		;;
	3.4|4.*)
		case $(tc-arch) in
			x86|amd64) filter-flags '-mcpu=*';;
			*-macos)
				# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=25127
				[[ ${GCC_BRANCH_VER} == 4.0 || ${GCC_BRANCH_VER}  == 4.1 ]] && \
					filter-flags '-mcpu=*' '-march=*' '-mtune=*'
			;;
		esac
		;;
	esac

	# Compile problems with these (bug #6641 among others)...
	#filter-flags "-fno-exceptions -fomit-frame-pointer -fforce-addr"

	# CFLAGS logic (verified with 3.4.3):
	# CFLAGS:
	#	This conflicts when creating a crosscompiler, so set to a sane
	#	  default in this case:
	#	used in ./configure and elsewhere for the native compiler
	#	used by gcc when creating libiberty.a
	#	used by xgcc when creating libstdc++ (and probably others)!
	#	  this behavior should be removed...
	#
	# CXXFLAGS:
	#	used by xgcc when creating libstdc++
	#
	# STAGE1_CFLAGS (not used in creating a crosscompile gcc):
	#	used by ${CHOST}-gcc for building stage1 compiler
	#
	# BOOT_CFLAGS (not used in creating a crosscompile gcc):
	#	used by xgcc for building stage2/3 compiler

	if is_crosscompile ; then
		# Set this to something sane for both native and target
		CFLAGS="-O2 -pipe"
		FFLAGS=${CFLAGS}
		FCFLAGS=${CFLAGS}

		local VAR="CFLAGS_"${CTARGET//-/_}
		CXXFLAGS=${!VAR}
	fi

	export GCJFLAGS=${GCJFLAGS:-${CFLAGS}}
}

toolchain_src_compile() {
	gcc_do_filter_flags
	einfo "CFLAGS=\"${CFLAGS}\""
	einfo "CXXFLAGS=\"${CXXFLAGS}\""

	# Force internal zip based jar script to avoid random
	# issues with 3rd party jar implementations.  #384291
	export JAR=no

	# For hardened gcc 4.3 piepatchset to build the hardened specs
	# file (build.specs) to use when building gcc.
	if ! tc_version_is_at_least 4.4 && want_minispecs ; then
		setup_minispecs_gcc_build_specs
	fi
	# Build in a separate build tree
	mkdir -p "${WORKDIR}"/build
	pushd "${WORKDIR}"/build > /dev/null

	einfo "Configuring ${PN} ..."
	gcc_do_configure

	touch "${S}"/gcc/c-gperf.h

	# Do not make manpages if we do not have perl ...
	[[ ! -x /usr/bin/perl ]] \
		&& find "${WORKDIR}"/build -name '*.[17]' | xargs touch

	einfo "Compiling ${PN} ..."
	gcc_do_make ${GCC_MAKE_TARGET}

	popd > /dev/null
}

toolchain_src_test() {
	cd "${WORKDIR}"/build
	emake -k check || ewarn "check failed and that sucks :("
}

toolchain_src_install() {
	local x=

	cd "${WORKDIR}"/build
	# Do allow symlinks in private gcc include dir as this can break the build
	find gcc/include*/ -type l -print0 | xargs -0 rm -f
	# Remove generated headers, as they can cause things to break
	# (ncurses, openssl, etc).
	for x in $(find gcc/include*/ -name '*.h') ; do
		grep -q 'It has been auto-edited by fixincludes from' "${x}" \
			&& rm -f "${x}"
	done
	# Do the 'make install' from the build directory
	S=${WORKDIR}/build \
	emake -j1 DESTDIR="${D}" install || die
	# Punt some tools which are really only useful while building gcc
	find "${D}" -name install-tools -prune -type d -exec rm -rf "{}" \;
	# This one comes with binutils
	find "${D}" -name libiberty.a -exec rm -f "{}" \;

	# Move the libraries to the proper location
	gcc_movelibs

	# Basic sanity check
	if ! is_crosscompile ; then
		local EXEEXT
		eval $(grep ^EXEEXT= "${WORKDIR}"/build/gcc/config.log)
		[[ -r ${D}${BINPATH}/gcc${EXEEXT} ]] || die "gcc not found in ${D}"
	fi

	dodir /etc/env.d/gcc
	create_gcc_env_entry

	# Setup the gcc_env_entry for hardened gcc 4 with minispecs
	if want_minispecs ; then
		copy_minispecs_gcc_specs
	fi
	# Make sure we dont have stuff lying around that
	# can nuke multiple versions of gcc

	gcc_slot_java

	# These should be symlinks
	dodir /usr/bin
	cd "${D}"${BINPATH}
	# Ugh: we really need to auto-detect this list.
	#      It's constantly out of date.
	for x in cpp gcc g++ c++ gcov g77 gcj gcjh gfortran gccgo ; do
		# For some reason, g77 gets made instead of ${CTARGET}-g77...
		# this should take care of that
		[[ -f ${x} ]] && mv ${x} ${CTARGET}-${x}

		if [[ -f ${CTARGET}-${x} ]] ; then
			if ! is_crosscompile ; then
				ln -sf ${CTARGET}-${x} ${x}
				dosym ${BINPATH}/${CTARGET}-${x} \
					/usr/bin/${x}-${GCC_CONFIG_VER}
			fi

			# Create version-ed symlinks
			dosym ${BINPATH}/${CTARGET}-${x} \
				/usr/bin/${CTARGET}-${x}-${GCC_CONFIG_VER}
		fi

		if [[ -f ${CTARGET}-${x}-${GCC_CONFIG_VER} ]] ; then
			rm -f ${CTARGET}-${x}-${GCC_CONFIG_VER}
			ln -sf ${CTARGET}-${x} ${CTARGET}-${x}-${GCC_CONFIG_VER}
		fi
	done

	# Now do the fun stripping stuff
	env RESTRICT="" CHOST=${CHOST} prepstrip "${D}${BINPATH}"
	env RESTRICT="" CHOST=${CTARGET} prepstrip "${D}${LIBPATH}"
	# gcc used to install helper binaries in lib/ but then moved to libexec/
	[[ -d ${D}${PREFIX}/libexec/gcc ]] && \
		env RESTRICT="" CHOST=${CHOST} prepstrip "${D}${PREFIX}/libexec/gcc/${CTARGET}/${GCC_CONFIG_VER}"

	cd "${S}"
	if is_crosscompile; then
		rm -rf "${D}"/usr/share/{man,info}
		rm -rf "${D}"${DATAPATH}/{man,info}
	else
		if tc_version_is_at_least 3.0 ; then
			local cxx_mandir=$(find "${WORKDIR}/build/${CTARGET}/libstdc++-v3" -name man)
			if [[ -d ${cxx_mandir} ]] ; then
				# clean bogus manpages #113902
				find "${cxx_mandir}" -name '*_build_*' -exec rm {} \;
				cp -r "${cxx_mandir}"/man? "${D}/${DATAPATH}"/man/
			fi
		fi
		has noinfo ${FEATURES} \
			&& rm -r "${D}/${DATAPATH}"/info \
			|| prepinfo "${DATAPATH}"
		has noman ${FEATURES} \
			&& rm -r "${D}/${DATAPATH}"/man \
			|| prepman "${DATAPATH}"
	fi
	# prune empty dirs left behind
	find "${D}" -depth -type d -delete 2>/dev/null

	# install testsuite results
	if use test; then
		docinto testsuite
		find "${WORKDIR}"/build -type f -name "*.sum" -print0 | xargs -0 dodoc
		find "${WORKDIR}"/build -type f -path "*/testsuite/*.log" -print0 \
			| xargs -0 dodoc
	fi

	# Rather install the script, else portage with changing $FILESDIR
	# between binary and source package borks things ....
	if ! is_crosscompile ; then
		insinto "${DATAPATH}"
		if tc_version_is_at_least 4.0 ; then
			newins "${GCC_FILESDIR}"/awk/fixlafiles.awk-no_gcc_la fixlafiles.awk || die
			find "${D}/${LIBPATH}" -name libstdc++.la -type f -exec rm "{}" \;
		else
			doins "${GCC_FILESDIR}"/awk/fixlafiles.awk || die
		fi
		exeinto "${DATAPATH}"
		doexe "${GCC_FILESDIR}"/fix_libtool_files.sh || die
		doexe "${GCC_FILESDIR}"/c{89,99} || die
	fi

	# Use gid of 0 because some stupid ports don't have
	# the group 'root' set to gid 0.  Send to /dev/null
	# for people who are testing as non-root.
	chown -R root:0 "${D}"${LIBPATH} 2>/dev/null

	# Move pretty-printers to gdb datadir to shut ldconfig up
	local py gdbdir=/usr/share/gdb/auto-load${LIBPATH/\/lib\//\/$(get_libdir)\/}
	pushd "${D}"${LIBPATH} >/dev/null
	for py in $(find . -name '*-gdb.py') ; do
		local multidir=${py%/*}
		insinto "${gdbdir}/${multidir}"
		sed -i "/^libdir =/s:=.*:= '${LIBPATH}/${multidir}':" "${py}" || die #348128
		doins "${py}" || die
		rm "${py}" || die
	done
	popd >/dev/null

	# Don't scan .gox files for executable stacks - false positives
	export QA_EXECSTACK="usr/lib*/go/*/*.gox"
	export QA_WX_LOAD="usr/lib*/go/*/*.gox"

	# Disable RANDMMAP so PCH works. #301299
	if tc_version_is_at_least 4.3 ; then
		pax-mark -r "${D}${PREFIX}/libexec/gcc/${CTARGET}/${GCC_CONFIG_VER}/cc1"
		pax-mark -r "${D}${PREFIX}/libexec/gcc/${CTARGET}/${GCC_CONFIG_VER}/cc1plus"
	fi
}

gcc_slot_java() {
	local x

	# Move Java headers to compiler-specific dir
	for x in "${D}"${PREFIX}/include/gc*.h "${D}"${PREFIX}/include/j*.h ; do
		[[ -f ${x} ]] && mv -f "${x}" "${D}"${LIBPATH}/include/
	done
	for x in gcj gnu java javax org ; do
		if [[ -d ${D}${PREFIX}/include/${x} ]] ; then
			dodir /${LIBPATH}/include/${x}
			mv -f "${D}"${PREFIX}/include/${x}/* "${D}"${LIBPATH}/include/${x}/
			rm -rf "${D}"${PREFIX}/include/${x}
		fi
	done

	if [[ -d ${D}${PREFIX}/lib/security ]] || [[ -d ${D}${PREFIX}/$(get_libdir)/security ]] ; then
		dodir /${LIBPATH}/security
		mv -f "${D}"${PREFIX}/lib*/security/* "${D}"${LIBPATH}/security
		rm -rf "${D}"${PREFIX}/lib*/security
	fi

	# Move random gcj files to compiler-specific directories
	for x in libgcj.spec logging.properties ; do
		x="${D}${PREFIX}/lib/${x}"
		[[ -f ${x} ]] && mv -f "${x}" "${D}"${LIBPATH}/
	done

	# Rename jar because it could clash with Kaffe's jar if this gcc is
	# primary compiler (aka don't have the -<version> extension)
	cd "${D}"${BINPATH}
	[[ -f jar ]] && mv -f jar gcj-jar
}

# Move around the libs to the right location.  For some reason,
# when installing gcc, it dumps internal libraries into /usr/lib
# instead of the private gcc lib path
gcc_movelibs() {
	# older versions of gcc did not support --print-multi-os-directory
	tc_version_is_at_least 3.2 || return 0

	local x multiarg removedirs=""
	for multiarg in $($(XGCC) -print-multi-lib) ; do
		multiarg=${multiarg#*;}
		multiarg=${multiarg//@/ -}

		local OS_MULTIDIR=$($(XGCC) ${multiarg} --print-multi-os-directory)
		local MULTIDIR=$($(XGCC) ${multiarg} --print-multi-directory)
		local TODIR=${D}${LIBPATH}/${MULTIDIR}
		local FROMDIR=

		[[ -d ${TODIR} ]] || mkdir -p ${TODIR}

		for FROMDIR in \
			${LIBPATH}/${OS_MULTIDIR} \
			${LIBPATH}/../${MULTIDIR} \
			${PREFIX}/lib/${OS_MULTIDIR} \
			${PREFIX}/${CTARGET}/lib/${OS_MULTIDIR}
		do
			removedirs="${removedirs} ${FROMDIR}"
			FROMDIR=${D}${FROMDIR}
			if [[ ${FROMDIR} != "${TODIR}" && -d ${FROMDIR} ]] ; then
				local files=$(find "${FROMDIR}" -maxdepth 1 ! -type d 2>/dev/null)
				if [[ -n ${files} ]] ; then
					mv ${files} "${TODIR}"
				fi
			fi
		done
		fix_libtool_libdir_paths "${LIBPATH}/${MULTIDIR}"

		# SLOT up libgcj.pc if it's available (and let gcc-config worry about links)
		FROMDIR="${PREFIX}/lib/${OS_MULTIDIR}"
		for x in "${D}${FROMDIR}"/pkgconfig/libgcj*.pc ; do
			[[ -f ${x} ]] || continue
			sed -i "/^libdir=/s:=.*:=${LIBPATH}/${MULTIDIR}:" "${x}"
			mv "${x}" "${D}${FROMDIR}"/pkgconfig/libgcj-${GCC_PV}.pc || die
		done
	done

	# We remove directories separately to avoid this case:
	#	mv SRC/lib/../lib/*.o DEST
	#	rmdir SRC/lib/../lib/
	#	mv SRC/lib/../lib32/*.o DEST  # Bork
	for FROMDIR in ${removedirs} ; do
		rmdir "${D}"${FROMDIR} >& /dev/null
	done
	find "${D}" -type d | xargs rmdir >& /dev/null
}

#----<< src_* >>----

#---->> unorganized crap in need of refactoring follows

# gcc_quick_unpack will unpack the gcc tarball and patches in a way that is
# consistant with the behavior of get_gcc_src_uri. The only patch it applies
# itself is the branch update if present.
#
# Travis Tilley <lv@gentoo.org> (03 Sep 2004)
#
gcc_quick_unpack() {
	pushd "${WORKDIR}" > /dev/null
	export PATCH_GCC_VER=${PATCH_GCC_VER:-${GCC_RELEASE_VER}}
	export UCLIBC_GCC_VER=${UCLIBC_GCC_VER:-${PATCH_GCC_VER}}
	export PIE_GCC_VER=${PIE_GCC_VER:-${GCC_RELEASE_VER}}
	export HTB_GCC_VER=${HTB_GCC_VER:-${GCC_RELEASE_VER}}
	export SPECS_GCC_VER=${SPECS_GCC_VER:-${GCC_RELEASE_VER}}

	if [[ -n ${GCC_A_FAKEIT} ]] ; then
		unpack ${GCC_A_FAKEIT}
	elif [[ -n ${PRERELEASE} ]] ; then
		unpack gcc-${PRERELEASE}.tar.bz2
	elif [[ -n ${SNAPSHOT} ]] ; then
		unpack gcc-${SNAPSHOT}.tar.bz2
	elif [[ ${PV} != *9999* ]] ; then
		unpack gcc-${GCC_RELEASE_VER}.tar.bz2
		# We want branch updates to be against a release tarball
		if [[ -n ${BRANCH_UPDATE} ]] ; then
			pushd "${S}" > /dev/null
			epatch "${DISTDIR}"/gcc-${GCC_RELEASE_VER}-branch-update-${BRANCH_UPDATE}.patch.bz2
			popd > /dev/null
		fi
	fi

	if [[ -n ${D_VER} ]] && use d ; then
		pushd "${S}"/gcc > /dev/null
		unpack gdc-${D_VER}-src.tar.bz2
		cd ..
		ebegin "Adding support for the D language"
		./gcc/d/setup-gcc.sh >& "${T}"/dgcc.log
		if ! eend $? ; then
			eerror "The D gcc package failed to apply"
			eerror "Please include this log file when posting a bug report:"
			eerror "  ${T}/dgcc.log"
			die "failed to include the D language"
		fi
		popd > /dev/null
	fi

	[[ -n ${PATCH_VER} ]] && \
		unpack gcc-${PATCH_GCC_VER}-patches-${PATCH_VER}.tar.bz2

	[[ -n ${UCLIBC_VER} ]] && \
		unpack gcc-${UCLIBC_GCC_VER}-uclibc-patches-${UCLIBC_VER}.tar.bz2

	if want_pie ; then
		if [[ -n ${PIE_CORE} ]] ; then
			unpack ${PIE_CORE}
		else
			unpack gcc-${PIE_GCC_VER}-piepatches-v${PIE_VER}.tar.bz2
		fi
		[[ -n ${SPECS_VER} ]] && \
			unpack gcc-${SPECS_GCC_VER}-specs-${SPECS_VER}.tar.bz2
	fi

	use_if_iuse boundschecking && unpack "bounds-checking-gcc-${HTB_GCC_VER}-${HTB_VER}.patch.bz2"

	popd > /dev/null
}

do_gcc_HTB_patches() {
	use_if_iuse boundschecking || return 0

	# modify the bounds checking patch with a regression patch
	epatch "${WORKDIR}/bounds-checking-gcc-${HTB_GCC_VER}-${HTB_VER}.patch"
	BRANDING_GCC_PKGVERSION="${BRANDING_GCC_PKGVERSION}, HTB-${HTB_GCC_VER}-${HTB_VER}"
}

# do various updates to PIE logic
do_gcc_PIE_patches() {
	want_pie || return 0

	use vanilla && return 0

	if tc_version_is_at_least 4.3.2; then
		guess_patch_type_in_dir "${WORKDIR}"/piepatch/
		EPATCH_MULTI_MSG="Applying pie patches ..." \
		epatch "${WORKDIR}"/piepatch/
	else
		guess_patch_type_in_dir "${WORKDIR}"/piepatch/upstream

		# corrects startfile/endfile selection and shared/static/pie flag usage
		EPATCH_MULTI_MSG="Applying upstream pie patches ..." \
		epatch "${WORKDIR}"/piepatch/upstream
		# adds non-default pie support (rs6000)
		EPATCH_MULTI_MSG="Applying non-default pie patches ..." \
		epatch "${WORKDIR}"/piepatch/nondef
		# adds default pie support (rs6000 too) if DEFAULT_PIE[_SSP] is defined
		EPATCH_MULTI_MSG="Applying default pie patches ..." \
		epatch "${WORKDIR}"/piepatch/def
	fi

	# we want to be able to control the pie patch logic via something other
	# than ALL_CFLAGS...
	sed -e '/^ALL_CFLAGS/iHARD_CFLAGS = ' \
		-e 's|^ALL_CFLAGS = |ALL_CFLAGS = $(HARD_CFLAGS) |' \
		-i "${S}"/gcc/Makefile.in
	# Need to add HARD_CFLAGS to ALL_CXXFLAGS on >= 4.7
	if tc_version_is_at_least 4.7.0 ; then
		sed -e '/^ALL_CXXFLAGS/iHARD_CFLAGS = ' \
                        -e 's|^ALL_CXXFLAGS = |ALL_CXXFLAGS = $(HARD_CFLAGS) |' \
                        -i "${S}"/gcc/Makefile.in
	fi

	BRANDING_GCC_PKGVERSION="${BRANDING_GCC_PKGVERSION}, pie-${PIE_VER}"
}

should_we_gcc_config() {
	# if the current config is invalid, we definitely want a new one
	# Note: due to bash quirkiness, the following must not be 1 line
	local curr_config
	curr_config=$(env -i ROOT="${ROOT}" gcc-config -c ${CTARGET} 2>&1) || return 0

	# if the previously selected config has the same major.minor (branch) as
	# the version we are installing, then it will probably be uninstalled
	# for being in the same SLOT, make sure we run gcc-config.
	local curr_config_ver=$(env -i ROOT="${ROOT}" gcc-config -S ${curr_config} | awk '{print $2}')

	local curr_branch_ver=$(get_version_component_range 1-2 ${curr_config_ver})

	# If we're using multislot, just run gcc-config if we're installing
	# to the same profile as the current one.
	use multislot && return $([[ ${curr_config_ver} == ${GCC_CONFIG_VER} ]])

	if [[ ${curr_branch_ver} == ${GCC_BRANCH_VER} ]] ; then
		return 0
	else
		# if we're installing a genuinely different compiler version,
		# we should probably tell the user -how- to switch to the new
		# gcc version, since we're not going to do it for him/her.
		# We don't want to switch from say gcc-3.3 to gcc-3.4 right in
		# the middle of an emerge operation (like an 'emerge -e world'
		# which could install multiple gcc versions).
		# Only warn if we're installing a pkg as we might be called from
		# the pkg_{pre,post}rm steps.  #446830
		if [[ ${EBUILD_PHASE} == *"inst" ]] ; then
			einfo "The current gcc config appears valid, so it will not be"
			einfo "automatically switched for you.  If you would like to"
			einfo "switch to the newly installed gcc version, do the"
			einfo "following:"
			echo
			einfo "gcc-config ${CTARGET}-${GCC_CONFIG_VER}"
			einfo "source /etc/profile"
			echo
		fi
		return 1
	fi
}

do_gcc_config() {
	if ! should_we_gcc_config ; then
		env -i ROOT="${ROOT}" gcc-config --use-old --force
		return 0
	fi

	local current_gcc_config="" current_specs="" use_specs=""

	current_gcc_config=$(env -i ROOT="${ROOT}" gcc-config -c ${CTARGET} 2>/dev/null)
	if [[ -n ${current_gcc_config} ]] ; then
		# figure out which specs-specific config is active
		current_specs=$(gcc-config -S ${current_gcc_config} | awk '{print $3}')
		[[ -n ${current_specs} ]] && use_specs=-${current_specs}
	fi
	if [[ -n ${use_specs} ]] && \
	   [[ ! -e ${ROOT}/etc/env.d/gcc/${CTARGET}-${GCC_CONFIG_VER}${use_specs} ]]
	then
		ewarn "The currently selected specs-specific gcc config,"
		ewarn "${current_specs}, doesn't exist anymore. This is usually"
		ewarn "due to enabling/disabling hardened or switching to a version"
		ewarn "of gcc that doesnt create multiple specs files. The default"
		ewarn "config will be used, and the previous preference forgotten."
		ebeep
		epause
		use_specs=""
	fi

	gcc-config ${CTARGET}-${GCC_CONFIG_VER}${use_specs}
}

# This function allows us to gentoo-ize gcc's version number and bugzilla
# URL without needing to use patches.
gcc_version_patch() {
	# gcc-4.3+ has configure flags (whoo!)
	tc_version_is_at_least 4.3 && return 0

	local version_string=${GCC_CONFIG_VER}
	[[ -n ${BRANCH_UPDATE} ]] && version_string+=" ${BRANCH_UPDATE}"

	einfo "patching gcc version: ${version_string} (${BRANDING_GCC_PKGVERSION})"

	local gcc_sed=( -e 's:gcc\.gnu\.org/bugs\.html:bugs\.gentoo\.org/:' )
	if grep -qs VERSUFFIX "${S}"/gcc/version.c ; then
		gcc_sed+=( -e "/VERSUFFIX \"\"/s:\"\":\" (${BRANDING_GCC_PKGVERSION})\":" )
	else
		version_string="${version_string} (${BRANDING_GCC_PKGVERSION})"
		gcc_sed+=( -e "/const char version_string\[\] = /s:= \".*\":= \"${version_string}\":" )
	fi
	sed -i "${gcc_sed[@]}" "${S}"/gcc/version.c || die
}

# This is a historical wart.  The original Gentoo/amd64 port used:
#    lib32 - 32bit binaries (x86)
#    lib64 - 64bit binaries (x86_64)
#    lib   - "native" binaries (a symlink to lib64)
# Most other distros use the logic (including mainline gcc):
#    lib   - 32bit binaries (x86)
#    lib64 - 64bit binaries (x86_64)
# Over time, Gentoo is migrating to the latter form.
#
# Unfortunately, due to distros picking the lib32 behavior, newer gcc
# versions will dynamically detect whether to use lib or lib32 for its
# 32bit multilib.  So, to keep the automagic from getting things wrong
# while people are transitioning from the old style to the new style,
# we always set the MULTILIB_OSDIRNAMES var for relevant targets.
setup_multilib_osdirnames() {
	is_multilib || return 0

	local config
	local libdirs="../lib64 ../lib32"

	# this only makes sense for some Linux targets
	case ${CTARGET} in
		x86_64*-linux*)    config="i386" ;;
		powerpc64*-linux*) config="rs6000" ;;
		sparc64*-linux*)   config="sparc" ;;
		s390x*-linux*)     config="s390" ;;
		*)	               return 0 ;;
	esac
	config+="/t-linux64"

	if [[ ${SYMLINK_LIB} == "yes" ]] ; then
		einfo "updating multilib directories to be: ${libdirs}"
		if tc_version_is_at_least 4.7 ; then
			set -- -e '/^MULTILIB_OSDIRNAMES.*lib32/s:[$][(]if.*):../lib32:'
		else
			set -- -e "/^MULTILIB_OSDIRNAMES/s:=.*:= ${libdirs}:"
		fi
	else
		einfo "using upstream multilib; disabling lib32 autodetection"
		set -- -r -e 's:[$][(]if.*,(.*)[)]:\1:'
	fi
	sed -i "$@" "${S}"/gcc/config/${config} || die
}

# make sure the libtool archives have libdir set to where they actually
# -are-, and not where they -used- to be.  also, any dependencies we have
# on our own .la files need to be updated.
fix_libtool_libdir_paths() {
	pushd "${D}" >/dev/null

	pushd "./${1}" >/dev/null
	local dir="${PWD#${D%/}}"
	local allarchives=$(echo *.la)
	allarchives="\(${allarchives// /\\|}\)"
	popd >/dev/null

	sed -i \
		-e "/^libdir=/s:=.*:='${dir}':" \
		./${dir}/*.la
	sed -i \
		-e "/^dependency_libs=/s:/[^ ]*/${allarchives}:${LIBPATH}/\1:g" \
		$(find ./${PREFIX}/lib* -maxdepth 3 -name '*.la') \
		./${dir}/*.la

	popd >/dev/null
}

is_multilib() {
	tc_version_is_at_least 3 || return 1
	use multilib
}

is_cxx() {
	gcc-lang-supported 'c++' || return 1
	use cxx
}

is_d() {
	gcc-lang-supported d || return 1
	use_if_iuse d
}

is_f77() {
	gcc-lang-supported f77 || return 1
	use fortran
}

is_f95() {
	gcc-lang-supported f95 || return 1
	use fortran
}

is_fortran() {
	gcc-lang-supported fortran || return 1
	use fortran
}

is_gcj() {
	gcc-lang-supported java || return 1
	use cxx && use_if_iuse gcj
}

is_go() {
	gcc-lang-supported go || return 1
	use cxx && use_if_iuse go
}

is_objc() {
	gcc-lang-supported objc || return 1
	use_if_iuse objc
}

is_objcxx() {
	gcc-lang-supported 'obj-c++' || return 1
	use cxx && use_if_iuse objc++
}

is_ada() {
	gcc-lang-supported ada || return 1
	use ada
}

is_treelang() {
	use_if_iuse boundschecking && return 1 #260532
	is_crosscompile && return 1 #199924
	gcc-lang-supported treelang || return 1
	#use treelang
	return 0
}

# should kill these off once all the ebuilds are migrated
gcc_pkg_setup() { toolchain_pkg_setup ; }
gcc_src_unpack() { toolchain_src_unpack ; }
gcc_src_compile() { toolchain_src_compile ; }
gcc_src_test() { toolchain_src_test ; }
