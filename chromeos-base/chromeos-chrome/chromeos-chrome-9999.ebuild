# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# Usage: by default, downloads chromium browser from the build server.
# If CHROME_ORIGIN is set to one of {SERVER_SOURCE, LOCAL_SOURCE, LOCAL_BINARY},
# the build comes from the chromimum source repository (gclient sync),
# build server, locally provided source, or locally provided binary.
# If you are using SERVER_SOURCE, a gclient template file that is in the files
# directory which will be copied automatically during the build and used as
# the .gclient for 'gclient sync'.
# If building from LOCAL_SOURCE or LOCAL_BINARY specifying BUILDTYPE
# will allow you to specify "Debug" or another build type; "Release" is
# the default.
# gclient is expected to be in ~/depot_tools if EGCLIENT is not set
# to gclient path.

EAPI="2"
inherit autotest-deponly binutils-funcs eutils flag-o-matic git-2 multilib toolchain-funcs

DESCRIPTION="Open-source version of Google Chrome web browser"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="-asan +build_tests +chrome_remoting chrome_internal chrome_pdf +chrome_debug -chrome_debug_tests -chrome_media -clang -component_build -content_shell -drm +gold hardfp +highdpi +nacl neon -ninja -oem_wallpaper -pgo_use -pgo_generate +reorder +runhooks +verbose widevine_cdm"

# Do not strip the nacl_helper_bootstrap binary because the binutils
# objcopy/strip mangles the ELF program headers.
# TODO(mcgrathr,vapier): This should be removed after portage's prepstrip
# script is changed to use eu-strip instead of objcopy and strip.
STRIP_MASK="*/nacl_helper_bootstrap"

# When we get to EAPI=4
# REQUIRED_USE=<pgo_generate and pgo_use are mutually exclusive>
REORDER_SUBDIR="reorder"

# Portage version without optional portage suffix.
CHROME_VERSION="${PV/_*/}"

CHROME_SRC="chrome-src"
if use chrome_internal; then
	CHROME_SRC="${CHROME_SRC}-internal"
fi

# CHROME_CACHE_DIR is used for storing output artifacts, and is always a
# regular directory inside the chroot (i.e. it's never mounted in, so it's
# always safe to use cp -al for these artifacts).
if [[ -z ${CHROME_CACHE_DIR} ]] ; then
	CHROME_CACHE_DIR="/var/cache/chromeos-chrome/${CHROME_SRC}"
fi
addwrite "${CHROME_CACHE_DIR}"

# CHROME_DISTDIR is used for storing the source code, if any source code
# needs to be unpacked at build time (e.g. in the SERVER_SOURCE scenario.)
# It will be mounted into the chroot, so it is never safe to use cp -al
# for these files.
if [[ -z ${CHROME_DISTDIR} ]] ; then
	CHROME_DISTDIR="${PORTAGE_ACTUAL_DISTDIR:-${DISTDIR}}/${CHROME_SRC}"
fi
addwrite "${CHROME_DISTDIR}"

# chrome destination directory
CHROME_DIR=/opt/google/chrome
D_CHROME_DIR="${D}/${CHROME_DIR}"
RELEASE_EXTRA_CFLAGS=()

USE_TCMALLOC="linux_use_tcmalloc=1"

# For compilation/local chrome
BUILDTYPE="${BUILDTYPE:-Release}"
BOARD="${BOARD:-${SYSROOT##/build/}}"
BUILD_OUT="${BUILD_OUT:-out_${BOARD}}"
# WARNING: We are using a symlink now for the build directory to work around
# command line length limits. This will cause problems if you are doing
# parallel builds of different boards/variants.
# Unsetting BUILD_OUT_SYM will revert this behavior
BUILD_OUT_SYM="c"

PGO_SUFFIX=".pgo.tar.bz2"
PGO_LOCATION="gs://chromeos-prebuilt/pgo-job/canonicals/"

add_pgo_arches() {
	local file a
	for a in "$@" ; do
		file="chromeos-chrome-${a}-${PV}${PGO_SUFFIX}"
		SRC_URI+="pgo_use? ( ${a}? ( ${PGO_LOCATION}${file} ) ) "
	done
}

RESTRICT="mirror"
add_pgo_arches x86 amd64 arm

TEST_FILES=("ffmpeg_tests" "video_decode_accelerator_unittest" "ppapi_example_video_decode")
PPAPI_TEST_FILES=(
	lib{32,64}
	mock_nacl_gdb
	nacl_test_data
	ppapi_nacl_tests_{newlib,glibc}.nmf
	ppapi_nacl_tests_{newlib,glibc}_{x32,x64,arm}.nexe
	test_case.html
	test_case.html.mock-http-headers
	test_page.css
	test_url_loader_data
)

RDEPEND="${RDEPEND}
	app-arch/bzip2
	>=app-i18n/ibus-1.4.99
	arm? ( virtual/opengles )
	dev-libs/atk
	dev-libs/glib
	dev-libs/nspr
	>=dev-libs/nss-3.12.2
	dev-libs/libxml2
	dev-libs/dbus-glib
	x11-libs/cairo
	drm? ( x11-libs/libxkbcommon )
	x11-libs/libXScrnSaver
	x11-libs/pango
	>=media-libs/alsa-lib-1.0.19
	media-libs/fontconfig
	media-libs/freetype
	media-libs/libpng
	media-libs/mesa
	>=media-sound/adhd-0.0.1-r310
	net-misc/wget
	sys-fs/udev
	sys-libs/zlib
	x11-libs/libXcomposite
	x11-libs/libXcursor
	x11-libs/libXrandr
	x11-libs/libXScrnSaver
	chrome_remoting? ( x11-libs/libXtst )
	x11-apps/setxkbmap
	!arm? ( x11-libs/libva )"

DEPEND="${DEPEND}
	${RDEPEND}
	arm? ( x11-drivers/opengles-headers )
	chromeos-base/protofiles
	>=dev-util/gperf-3.0.3
	>=dev-util/pkgconfig-0.23
	net-wireless/bluez"

PATCHES=()

AUTOTEST_COMMON="src/chrome/test/chromeos/autotest/files"
AUTOTEST_DEPS="${AUTOTEST_COMMON}/client/deps"
AUTOTEST_DEPS_LIST="chrome_test pyauto_dep page_cycler_dep"

IUSE="${IUSE} +autotest"

export CHROMIUM_HOME=/usr/$(get_libdir)/chromium-browser

QA_TEXTRELS="*"
QA_EXECSTACK="*"
QA_PRESTRIPPED="*"

use_nacl() {
	! use asan && ! use component_build && ! use drm && use nacl
}

# Like the `usex` helper:
# Usage: echox [int] [echo-if-true] [echo-if-false]
# If [int] is 0, then echo the 2nd arg (default of yes), else
# echo the 3rd arg (default of no).
echox() {
	# Like the `usex` helper.
	[[ ${1:-$?} -eq 0 ]] && echo "${2:-yes}" || echo "${3:-no}"
}
echo10() { echox ${1:-$?} 1 0 ; }
echotf() { echox ${1:-$?} true false ; }
use10()  { usex $1 1 0 ; }
usetf()  { usex $1 true false ; }
set_build_defines() {
	# General build defines.
	# TODO(vapier): Check that this should say SYSROOT not ROOT
	BUILD_DEFINES=(
		"sysroot=${ROOT}"
		python_ver=2.6
		"linux_sandbox_path=${CHROME_DIR}/chrome-sandbox"
		"${EXTRA_BUILD_ARGS}"
		"system_libdir=$(get_libdir)"
		"pkg-config=$(tc-getPKG_CONFIG)"
		"use_xi2_mt=2"
	)

	BUILD_DEFINES+=(
		swig_defines=-DOS_CHROMEOS
		chromeos=1
	)

	if use pgo_generate ; then
		BUILD_DEFINES+=(
			libraries_for_target=-lgcov
		)
		RELEASE_EXTRA_CFLAGS+=(
			-DPGO_GENERATE
			-fprofile-generate
			-fprofile-dir=/tmp/pgo/chrome
			-Wno-error=maybe-uninitialized
		)
	fi

	# Set proper BUILD_DEFINES for the arch
	case "${ARCH}" in
	x86)
		BUILD_DEFINES+=( target_arch=ia32 enable_smooth_scrolling=1 )
		;;
	arm)
		BUILD_DEFINES+=(
			target_arch=arm
			arm_float_abi=$(usex hardfp hard softfp)
			arm_neon=$(use10 neon)
			armv7=$([[ ${CHOST} == armv7* ]]; echo10)
			v8_can_use_unaligned_accesses=$([[ ${CHOST} == armv[67]* ]]; echotf)
			v8_can_use_vfp3_instructions=$([[ ${ARM_FPU} == *vfpv[34]* ]]; echotf)
			v8_use_arm_eabi_hardfloat=$(usetf hardfp)
		)
		if [[ -n "${ARM_FPU}" ]]; then
			BUILD_DEFINES+=( arm_fpu="${ARM_FPU}" )
		fi
		if use chrome_internal; then
			#http://code.google.com/p/chrome-os-partner/issues/detail?id=1142
			BUILD_DEFINES+=( internal_pdf=0 )
		fi
		;;
	amd64)
		BUILD_DEFINES+=( target_arch=x64 enable_smooth_scrolling=1 )
		;;
	*)
		die "Unsupported architecture: ${ARCH}"
		;;
	esac

	use_nacl || BUILD_DEFINES+=( disable_nacl=1 )

	use drm && BUILD_DEFINES+=( use_drm=1 )

	# Control inclusion of optional chrome features.
	if use chrome_remoting; then
		BUILD_DEFINES+=( remoting=1 )
	else
		BUILD_DEFINES+=( remoting=0 )
	fi

	if use chrome_internal; then
		#Adding chrome branding specific variables and GYP_DEFINES
		BUILD_DEFINES+=( branding=Chrome buildtype=Official )
		export CHROMIUM_BUILD='_google_Chrome'
		export OFFICIAL_BUILD='1'
		export CHROME_BUILD_TYPE='_official'

		# For internal builds, don't remove webcore debug symbols by default.
		REMOVE_WEBCORE_DEBUG_SYMBOLS=${REMOVE_WEBCORE_DEBUG_SYMBOLS:-0}
	elif use chrome_media; then
		echo "Building Chromium with additional media codecs and containers."
		BUILD_DEFINES+=( ffmpeg_branding=ChromeOS proprietary_codecs=1 )
	fi

	# This saves time and bytes.
	if [[ "${REMOVE_WEBCORE_DEBUG_SYMBOLS:-1}" == "1" ]]; then
		BUILD_DEFINES+=( remove_webcore_debug_symbols=1 )
	fi

	if ! use chrome_debug_tests; then
		BUILD_DEFINES+=( strip_tests=1 )
	fi

	if use reorder && ! use clang; then
		BUILD_DEFINES+=( "order_text_section=${CHROME_DISTDIR}/${REORDER_SUBDIR}/section-ordering-files/orderfile" )
	fi

	if use clang; then
		if [[ "${ARCH}" == "x86" || "${ARCH}" == "amd64" ]]; then
			BUILD_DEFINES+=( clang=1 clang_use_chrome_plugins=0 werror= )
			USE_TCMALLOC="linux_use_tcmalloc=0"

			# The chrome build system will add -m32 for 32bit arches, and
			# clang defaults to 64bit because our cros_sdk is 64bit default.
			export CC="clang" CXX="clang++"
		else
			die "Clang is not yet supported for ${ARCH}"
		fi
	fi

	if use asan; then
		if ! use clang; then
			eerror "Asan requires Clang to run."
			die "Please set USE=\"${USE} clang\" to enable Clang"
		fi
		BUILD_DEFINES+=( asan=1 )
	fi

	if use component_build; then
		BUILD_DEFINES+=( component=shared_library )
	fi

	BUILD_DEFINES+=( "${USE_TCMALLOC}" )
	BUILD_DEFINES+=( "use_cras=1" )

	# TODO(davidjames): Pass in all CFLAGS this way, once gyp is smart enough
	# to accept cflags that only apply to the target.
	if use chrome_debug; then
		RELEASE_EXTRA_CFLAGS+=(
			-g
		)
	fi

	if ! use chrome_pdf; then
		BUILD_DEFINES+=( internal_pdf=0 )
	fi

	if use oem_wallpaper; then
		BUILD_DEFINES+=( use_oem_wallpaper=1 )
	fi

	BUILD_DEFINES+=( "release_extra_cflags='${RELEASE_EXTRA_CFLAGS[*]}'" )

	export GYP_GENERATORS="$(usex ninja ninja make)"
	export GYP_DEFINES="${BUILD_DEFINES[*]}"
	export builddir_name="${BUILD_OUT}"
	# Prevents gclient from updating self.
	export DEPOT_TOOLS_UPDATE=0
	# Chrome's build overrides the Chrome OS build wrapper's addition
	# of -fstack-protector-strong, so re-add it to the end of the
	# command-line, since gcc uses "last flag wins" for stack protector.
	CXXFLAGS+=" -fstack-protector-strong"
	# Enable std::vector []-operator bounds checking.
	CXXFLAGS+=" -D__google_stl_debug_vector=1"
}

unpack_chrome() {
	local cmd=( "${CROS_WORKON_SRCROOT}"/chromite/bin/sync_chrome )
	use chrome_internal && cmd+=( --internal )
	use chrome_pdf && cmd+=( --pdf )
	if [[ -n "${CROS_SVN_COMMIT}" ]]; then
		cmd+=( --revision="${CROS_SVN_COMMIT}" )
	elif [[ "${CHROME_VERSION}" != "9999" ]]; then
		cmd+=( --tag="${CHROME_VERSION}" )
	fi
	# --reset tells sync_chrome to blow away local changes and to feel
	# free to delete any directories that get in the way of syncing. This
	# is needed for unattended operation.
	cmd+=( --reset --gclient="${EGCLIENT}" "${CHROME_DISTDIR}" )
	elog "${cmd[*]}"
	"${cmd[@]}" || die
}

decide_chrome_origin() {
	local chrome_workon="=chromeos-base/chromeos-chrome-9999"
	local cros_workon_file="${ROOT}etc/portage/package.keywords/cros-workon"
	if [[ -e "${cros_workon_file}" ]] && grep -q "${chrome_workon}" "${cros_workon_file}"; then
		# LOCAL_SOURCE is the default for cros_workon
		# Warn the user if CHROME_ORIGIN is already set
		if [[ -n "${CHROME_ORIGIN}" && "${CHROME_ORIGIN}" != LOCAL_SOURCE ]]; then
			ewarn "CHROME_ORIGIN is already set to ${CHROME_ORIGIN}."
			ewarn "This will prevent you from building from your local checkout."
			ewarn "Please run 'unset CHROME_ORIGIN' to reset Chrome"
			ewarn "to the default source location."
		fi
		: ${CHROME_ORIGIN:=LOCAL_SOURCE}
	else
		# By default, pull from server
		: ${CHROME_ORIGIN:=SERVER_SOURCE}
	fi
}

sandboxless_ensure_directory() {
	local dir
	for dir in "$@"; do
		if [[ ! -d "${dir}" ]] ; then
			# We need root access to create these directories, so we need to
			# use sudo. This implicitly disables the sandbox.
			sudo mkdir -p "${dir}" || die
			sudo chown "${PORTAGE_USERNAME}:portage" "${dir}" || die
			sudo chmod 0755 "${dir}" || die
		fi
	done
}

src_unpack() {
	tc-export CC CXX
	local WHOAMI=$(whoami)
	export EGCLIENT="${EGCLIENT:-/home/${WHOAMI}/depot_tools/gclient}"
	export ENINJA="${ENINJA:-/home/${WHOAMI}/depot_tools/ninja}"
	export DEPOT_TOOLS_UPDATE=0

	# Create storage directories.
	sandboxless_ensure_directory "${CHROME_DISTDIR}" "${CHROME_CACHE_DIR}"

	# Copy in credentials to fake home directory so that build process
	# can access svn and ssh if needed.
	mkdir -p ${HOME}
	SUBVERSION_CONFIG_DIR=/home/${WHOAMI}/.subversion
	if [[ -d ${SUBVERSION_CONFIG_DIR} ]]; then
		cp -rfp ${SUBVERSION_CONFIG_DIR} ${HOME} || die
	fi
	SSH_CONFIG_DIR=/home/${WHOAMI}/.ssh
	if [[ -d ${SSH_CONFIG_DIR} ]]; then
		cp -rfp ${SSH_CONFIG_DIR} ${HOME} || die
	fi
	NET_CONFIG=/home/${WHOAMI}/.netrc
	if [[ -f ${NET_CONFIG} ]]; then
		cp -fp ${NET_CONFIG} ${HOME} || die
	fi

	decide_chrome_origin

	case "${CHROME_ORIGIN}" in
	LOCAL_SOURCE|SERVER_SOURCE|LOCAL_BINARY|GERRIT_SOURCE)
		elog "CHROME_ORIGIN VALUE is ${CHROME_ORIGIN}"
		;;
	*)
	die "CHROME_ORIGIN not one of LOCAL_SOURCE, SERVER_SOURCE, LOCAL_BINARY, GERRIT_SOURCE"
		;;
	esac

	# Prepare and set CHROME_ROOT based on CHROME_ORIGIN.
	# CHROME_ROOT is the location where the source code is used for compilation.
	# If we're in SERVER_SOURCE mode, CHROME_ROOT is CHROME_DISTDIR. In LOCAL_SOURCE
	# mode, this directory may be set manually to any directory. It may be mounted
	# into the chroot, so it is not safe to use cp -al for these files.
	# These are set here because $(whoami) returns the proper user here,
	# but 'root' at the root level of the file
	case "${CHROME_ORIGIN}" in
	(SERVER_SOURCE)
		elog "Using CHROME_VERSION = ${CHROME_VERSION}"
		unpack_chrome

		elog "set the chrome source root to ${CHROME_DISTDIR}"
		elog "From this point onwards there is no difference between \
			SERVER_SOURCE and LOCAL_SOURCE, since the fetch is done"
		CHROME_ROOT=${CHROME_DISTDIR}
		;;
	(GERRIT_SOURCE)
		CHROME_ROOT="/home/${WHOAMI}/trunk/chromium"
		# TODO(rcui): Remove all these addwrite hacks once we start
		# building off a copy of the source
		addwrite "${CHROME_ROOT}"
		# Addwrite to .repo because each project's .git directory links
		# to the .repo directory.
		addwrite "/home/${WHOAMI}/trunk/.repo/"
		# - Make the symlinks from chromium src tree to CrOS source tree
		# writeable so we can run hooks and reset the checkout.
		# - We need to explicitly do this because the symlink points to
		# outside of the CHROME_ROOT.
		# - We don't know which one is a symlink so do it for
		#   all files/directories in src/third_party
		# - chrome_set_ver creates symlinks in src/third_party to simulate
		#   the cros_deps checkout gclient does.  For details, see
		#   http://gerrit.chromium.org/gerrit/#change,5692.
		THIRD_PARTY_DIR="${CHROME_ROOT}/src/third_party"
		for f in `ls -1 ${THIRD_PARTY_DIR}`
		do
			addwrite "${THIRD_PARTY_DIR}/${f}"
		done
		;;
	(LOCAL_SOURCE)
		: ${CHROME_ROOT:=/home/${WHOAMI}/chrome_root}
		if [[ ! -d "${CHROME_ROOT}/src" ]]; then
			die "${CHROME_ROOT} does not contain a valid chromium checkout!"
		fi
		addwrite "${CHROME_ROOT}"
		;;
	esac

	case "${CHROME_ORIGIN}" in
	LOCAL_SOURCE|SERVER_SOURCE|GERRIT_SOURCE)
		set_build_defines
		;;
	esac

	# FIXME: This is the normal path where ebuild stores its working data.
	# Chrome builds inside distfiles because of speed, so we at least make
	# a symlink here to add compatibility with autotest eclass which uses this.
	ln -sf "${CHROME_ROOT}" "${WORKDIR}/${P}"

	if use pgo_use && ! use clang; then
		local PROFILE_DIR="${WORKDIR}/pgo"
		mkdir "${PROFILE_DIR}"
		cd "${PROFILE_DIR}"
		unpack "chromeos-chrome-${ARCH}-${PV}${PGO_SUFFIX}"
		cd "${WORKDIR}"
		if [[ -d "${PROFILE_DIR}/chrome" ]]; then
			append-flags -fprofile-use \
				-fprofile-correction \
				-fprofile-dir="${PROFILE_DIR}/chrome"

			# This is required because gcc emits different warnings
			# for PGO vs. non-PGO. PGO may inline different
			# functions from non-PGO, leading to different warnings.
			# crbug.com/112908
			append-flags -Wno-error=maybe-uninitialized
			einfo "Using the PGO data"
		else
			einfo "USE=+pgo_use, but ${PROFILE_DIR}/chrome not "\
			      "created from pgo.tar.bz2."
			die "PGO data supply failed"
		fi
	fi

	if use reorder && ! use clang; then
		EGIT_REPO_URI="http://git.chromium.org/chromiumos/profile/chromium.git"
		EGIT_COMMIT="88c20da2d91098449373f2e2f5cef35c193b8add"
		EGIT_PROJECT="${PN}-reorder"
		if grep -qs ${EGIT_COMMIT} "${CHROME_DISTDIR}/${REORDER_SUBDIR}/.git/HEAD"; then
			einfo "Reorder profile repo is up to date."
		else
			einfo "Reorder profile repo not up-to-date. Fetching..."
			EGIT_SOURCEDIR="${CHROME_DISTDIR}/${REORDER_SUBDIR}"
			rm -rf "${EGIT_SOURCEDIR}"
			git-2_src_unpack
		fi
	fi
}

src_prepare() {
	if [[ "${CHROME_ORIGIN}" != "LOCAL_SOURCE" &&
	      "${CHROME_ORIGIN}" != "SERVER_SOURCE" &&
	      "${CHROME_ORIGIN}" != "GERRIT_SOURCE" ]]; then
		return
	fi

	elog "${CHROME_ROOT} should be set here properly"
	cd "${CHROME_ROOT}/src" || die "Cannot chdir to ${CHROME_ROOT}"

	# We do symlink creation here if appropriate
	mkdir -p "${CHROME_CACHE_DIR}/src/${BUILD_OUT}"
	if [[ ! -z "${BUILD_OUT_SYM}" ]]; then
		rm -rf "${BUILD_OUT_SYM}" || die "Could not remove symlink"
		ln -sfT "${CHROME_CACHE_DIR}/src/${BUILD_OUT}" "${BUILD_OUT_SYM}" ||
			die "Could not create symlink for output directory"
		export builddir_name="${BUILD_OUT_SYM}"
	fi


	# Apply patches for non-localsource builds
	if [[ "${CHROME_ORIGIN}" == "SERVER_SOURCE" && ${#PATCHES[@]} -gt 0 ]]; then
		epatch "${PATCHES[@]}"
	fi

	# The chrome makefiles specify -O and -g flags already, so remove the
	# portage flags.
	filter-flags -g -O*

	local WHOAMI=$(whoami)
	# The hooks may depend on the environment variables we set in this
	# ebuild (i.e., GYP_DEFINES for gyp_chromium)
	ECHROME_SET_VER=${ECHROME_SET_VER:=/home/${WHOAMI}/trunk/chromite/bin/chrome_set_ver}
	einfo "Building Chrome with the following define options:"
	local opt
	for opt in "${BUILD_DEFINES[@]}"; do
		einfo "${opt}"
	done

	# Get credentials to fake home directory so that built chromium
	# can access Google services.
	# First check for Chrome credentials.
	if [[ ! -d google_apis/internal ]]; then
		# Then look for ChromeOS supplied credentials.
		local PRIVATE_OVERLAYS_DIR=/home/${WHOAMI}/trunk/src/private-overlays
		local GAPI_CONFIG_FILE=${PRIVATE_OVERLAYS_DIR}/chromeos-overlay/googleapikeys
		# RE to match the allowed names.
		local NRE="('google_(api_key|default_client_(id|secret))')"
		# RE to match whitespace.
		local WS="[[:space:]]*"
		# RE to match allowed values.
		local CRE="('[^\\\\']*')"
		# And combining them into one RE for describing the lines
		# we want to allow.
		local TRE="^${WS}${NRE}${WS}[:=]${WS}${CRE}.*"
		if [[ ! -f "${GAPI_CONFIG_FILE}" ]]; then
			# Then developer credentials.
			GAPI_CONFIG_FILE=/home/${WHOAMI}/.googleapikeys
		fi
		if [[ -f "${GAPI_CONFIG_FILE}" ]]; then
			mkdir "${HOME}"/.gyp
			cat <<-EOF >"${HOME}/.gyp/include.gypi"
			{
				'variables': {
				$(sed -nr -e "/^${TRE}/{s//\1: \4,/;p;}" \
					"${GAPI_CONFIG_FILE}")
				}
			}
			EOF
		fi
	fi
}

src_configure() {
	tc-export CXX CC AR AS RANLIB
	export CC_host=$(tc-getBUILD_CC)
	export CXX_host=$(tc-getBUILD_CXX)
	export AR_host=$(tc-getBUILD_AR)
	if use gold ; then
		if [[ "${GOLD_SET}" != "yes" ]]; then
			export GOLD_SET="yes"
			einfo "Using gold from the following location: $(get_binutils_path_gold)"
			export CC="${CC} -B$(get_binutils_path_gold)"
			export CXX="${CXX} -B$(get_binutils_path_gold)"
		fi
	else
		ewarn "gold disabled. Using GNU ld."
	fi

	# Use g++ as the linker driver.
	export LD="${CXX}"
	export LD_host=$(tc-getBUILD_CXX)

	local build_tool_flags=()
	if use ninja; then
		build_tool_flags+=(
			"config=${BUILDTYPE}"
			"output_dir=${builddir_name}"
		)
	fi
	export GYP_GENERATOR_FLAGS="${build_tool_flags[*]}"

	# TODO(rcui): crosbug.com/20435.  Investigate removal of runhooks
	# useflag when chrome build switches to Ninja inside the chroot.
	if use runhooks; then
		if [[ "${CHROME_ORIGIN}" == "GERRIT_SOURCE" ]]; then
			# Set the dependency repos to the revision specified in the
			# .DEPS.git file, and run the hooks in that file.
			"${ECHROME_SET_VER}" --runhooks || die
		else
			[[ -n "${EGCLIENT}" ]] || die EGCLIENT unset
			[[ -f "${EGCLIENT}" ]] || die EGCLIENT at "${EGCLIENT}" does not exist
			"${EGCLIENT}" runhooks --force || die  "Failed to run  ${EGCLIENT} runhooks"
		fi
	elif [[ "${CHROME_ORIGIN}" == "GERRIT_SOURCE" ]]; then
		"${ECHROME_SET_VER}" || die
	fi
}

chrome_make() {
	if use ninja; then
		PATH=${PATH}:/home/$(whoami)/depot_tools ${ENINJA} \
			${MAKEOPTS} -C "${BUILD_OUT_SYM}/${BUILDTYPE}" $(usex verbose -v "") "$@" || die
	else
		emake -r $(usex verbose V=1 "") "BUILDTYPE=${BUILDTYPE}" "$@" || die
	fi
}

src_compile() {
	if [[ "${CHROME_ORIGIN}" != "LOCAL_SOURCE" &&
	      "${CHROME_ORIGIN}" != "SERVER_SOURCE" &&
	      "${CHROME_ORIGIN}" != "GERRIT_SOURCE" ]]; then
		return
	fi

	cd "${CHROME_ROOT}"/src || die "Cannot chdir to ${CHROME_ROOT}/src"

	local chrome_targets=()

	if [[ -n "${CHROME_TARGET_OVERRIDE}" ]]; then
		chrome_targets=( ${CHROME_TARGET_OVERRIDE} )
		einfo "Building custom targets: ${chrome_targets[*]}"
	elif use content_shell; then
		chrome_targets=( content_shell chrome_sandbox )
		einfo "Building content_shell"
	else
		chrome_targets=( chrome chrome_sandbox )
		if use build_tests; then
			chrome_targets+=( "${TEST_FILES[@]}"
				pyautolib
				peerconnection_server
				chromedriver
				browser_tests
				sync_integration_tests )
			einfo "Building test targets: ${TEST_TARGETS[@]}"
		fi

		# The default_extensions target is a no-op for external builds, and is
		# broken with Ninja in this situation. For now, only enable it on
		# builds where it installs something.
		if use chrome_internal; then
			chrome_targets+=( default_extensions )
		fi

		if use_nacl; then
			chrome_targets+=( nacl_helper_bootstrap nacl_helper )
		fi

		if use drm; then
			chrome_targets+=( aura_demo ash_shell )
		else
			chrome_targets+=( libosmesa.so )
		fi
	fi

	chrome_make "${chrome_targets[@]}"

	if use build_tests; then
		install_chrome_test_resources "${WORKDIR}/test_src"
		install_pyauto_dep_resources "${WORKDIR}/pyauto_src"
		install_page_cycler_dep_resources "${WORKDIR}/page_cycler_src"

		# NOTE: Since chrome is built inside distfiles, we have to get
		# rid of the previous instance first.
		# We remove only what we will overwrite with the mv below.
		local deps="${WORKDIR}/${P}/${AUTOTEST_DEPS}"

		rm -rf "${deps}/chrome_test/test_src"
		mv "${WORKDIR}/test_src" "${deps}/chrome_test/"

		rm -rf "${deps}/pyauto_dep/test_src"
		mv "${WORKDIR}/pyauto_src" "${deps}/pyauto_dep/test_src"

		rm -rf "${deps}/page_cycler_dep/test_src"
		mv "${WORKDIR}/page_cycler_src" "${deps}/page_cycler_dep/test_src"

		# HACK: It would make more sense to call autotest_src_prepare in
		# src_prepare, but we need to call install_chrome_test_resources first.
		autotest-deponly_src_prepare

		# Remove .svn dirs
		esvn_clean "${AUTOTEST_WORKDIR}"

		autotest_src_compile
	fi
}

# Turn off the cp -l behavior in autotest, since the source dir and the
# installation dir live on different bind mounts right now.
fast_cp() {
	cp "$@"
}

install_test_resources() {
	# Install test resources from chrome source directory to destination.
	# We keep a cache of test resources inside the chroot to avoid copying
	# multiple times.
	local test_dir="${1}"
	shift
	local resource cache dest
	for resource in "$@"; do
		cache=$(dirname "${CHROME_CACHE_DIR}/src/${resource}")
		dest=$(dirname "${test_dir}/${resource}")
		mkdir -p "${cache}" "${dest}"
		rsync -a --delete --exclude=.svn \
			"${CHROME_ROOT}/src/${resource}" "${cache}"
		cp -al "${CHROME_CACHE_DIR}/src/${resource}" "${dest}"
	done
}

install_chrome_test_resources() {
	# NOTE: This is a duplicate from src_install, because it's required here.
	local from="${CHROME_CACHE_DIR}/src/${BUILD_OUT}/${BUILDTYPE}"
	local test_dir="${1}"

	echo Copying Chrome tests into "${test_dir}"
	mkdir -p "${test_dir}/out/Release"

	# Even if chrome_debug_tests is enabled, we don't need to include detailed
	# debug info for tests in the binary package, so save some time by stripping
	# everything but the symbol names. Developers who need more detailed debug
	# info on the tests can use the original unstripped tests from the ${from}
	# directory.
	TEST_INSTALL_TARGETS=(
		"libppapi_tests.so"
		"browser_tests"
		"ffmpeg_tests"
		"peerconnection_server"
		"sync_integration_tests"
		"video_decode_accelerator_unittest" )

	for f in "${TEST_INSTALL_TARGETS[@]}"; do
		$(tc-getSTRIP) --strip-debug \
			--keep-file-symbols "${from}"/${f} \
			-o "${test_dir}/out/Release/$(basename ${f})"
	done

	# Copy over the test data directory; eventually 'all' non-static
	# Chrome test data will go in here.
	mkdir "${test_dir}"/out/Release/test_data
	cp -al "${from}"/test_data "${test_dir}"/out/Release/

	# Add the fake bidi locale
	mkdir "${test_dir}"/out/Release/pseudo_locales
	cp -al "${from}"/pseudo_locales/fake-bidi.pak \
		"${test_dir}"/out/Release/pseudo_locales

	for f in "${TEST_FILES[@]}"; do
		cp -al "${from}/${f}" "${test_dir}"
	done

	for f in "${PPAPI_TEST_FILES[@]}"; do
		cp -al "${from}/${f}" "${test_dir}/out/Release"
	done

	# Install Chrome test resources.
	install_test_resources "${test_dir}" \
		base/base_paths_posix.cc \
		chrome/test/data \
		chrome/test/functional \
		chrome/third_party/mock4js/mock4js.js  \
		content/common/gpu/testdata \
		content/test/data \
		net/data/ssl/certificates \
		ppapi/tests/test_case.html \
		ppapi/tests/test_url_loader_data \
		third_party/bidichecker/bidichecker_packaged.js \
		third_party/WebKit/Tools/Scripts \
		third_party/WebKit/LayoutTests/http/tests/websocket/tests

	# Add pdf test data
	if use chrome_pdf; then
		install_test_resources "${test_dir}" pdf/test
	fi

	# Remove test binaries from other platforms
	if [[ -z "${E_MACHINE}" ]]; then
		echo E_MACHINE not defined!
	else
		cd "${test_dir}"/chrome/test
		rm -fv $( scanelf -RmyBF%a . | grep -v -e ^${E_MACHINE} )
	fi

	cp -a "${CHROME_ROOT}"/"${AUTOTEST_DEPS}"/chrome_test/setup_test_links.sh \
		"${test_dir}"/out/Release
	# Symlinks to resources in pyauto_dep will be created at runtime.
}

# Set up the PyAuto files also by copying out the files needed for that.
# We create a separate dependency because the chrome_test one is about 350MB
# and PyAuto is a svelte 30MB.
install_pyauto_dep_resources() {
	# NOTE: This is a duplicate from src_install, because it's required here.
	local from="${CHROME_CACHE_DIR}/src/${BUILD_OUT}/${BUILDTYPE}"
	local test_dir="${1}"

	echo "Copying PyAuto framework into ${test_dir}"

	mkdir -p "${test_dir}/out/Release"

	cp -al "${from}"/pyproto "${test_dir}"/out/Release
	cp -al "${from}"/pyautolib.py "${test_dir}"/out/Release

	# Even if chrome_debug_tests is enabled, we don't need to include
	# detailed debug info for tests in the binary package, so save some
	# time by stripping everything but the symbol names. Developers who
	# need more detailed debug info on the tests can use the original
	# unstripped tests from the ${from} directory.
	$(tc-getSTRIP) --strip-debug \
		--keep-file-symbols "${from}"/_pyautolib.so \
		-o "${test_dir}"/out/Release/_pyautolib.so
	$(tc-getSTRIP) --strip-debug \
		--keep-file-symbols "${from}"/chromedriver \
		-o "${test_dir}"/out/Release/chromedriver
	if use component_build; then
		mkdir -p "${test_dir}/out/Release/lib.target"
		local src dst
		for src in "${from}"/lib.target/* ; do
			dst="${test_dir}/out/Release/${src#${from}}"
			$(tc-getSTRIP) --strip-debug --keep-file-symbols \
				"${src}" -o "${dst}"
		done
	fi

	cp -a "${CHROME_ROOT}"/"${AUTOTEST_DEPS}"/pyauto_dep/setup_test_links.sh \
		"${test_dir}"/out/Release

	# Copy PyAuto scripts and suppport libs.
	install_test_resources "${test_dir}" \
		chrome/browser/resources/gaia_auth \
		chrome/test/pyautolib \
		net/tools/testserver \
		third_party/pyftpdlib \
		third_party/pywebsocket \
		third_party/simplejson \
		third_party/tlslite \
		third_party/webdriver
}

install_page_cycler_dep_resources() {
	local test_dir="${1}"

	if [[ -r "${CHROME_ROOT}/src/data/page_cycler" ]]; then
		echo "Copying Page Cycler Data into ${test_dir}"
		mkdir -p "${test_dir}"
		install_test_resources "${test_dir}" \
			data/page_cycler
	fi
}

src_install() {
	FROM="${CHROME_CACHE_DIR}/src/${BUILD_OUT}/${BUILDTYPE}"

	# Override default strip flags and lose the '-R .comment'
	# in order to play nice with the crash server.
	if [[ -z "${KEEP_CHROME_DEBUG_SYMBOLS}" ]]; then
		export PORTAGE_STRIP_FLAGS="--strip-unneeded"
	else
		export PORTAGE_STRIP_FLAGS="--strip-debug --keep-file-symbols"
	fi

	# First, things from the chrome build output directory
	dodir "${CHROME_DIR}"
	dodir "${CHROME_DIR}"/plugins

	exeinto "${CHROME_DIR}"
	doexe "${FROM}"/chrome
	doexe "${FROM}"/libffmpegsumo.so
	doexe "${FROM}"/libosmesa.so
	use content_shell && doexe "${FROM}"/content_shell
	use drm && doexe "${FROM}"/aura_demo
	use drm && doexe "${FROM}"/ash_shell
	if use chrome_internal && use chrome_pdf; then
		doexe "${FROM}"/libpdf.so
	fi
	if use chrome_internal && use widevine_cdm; then
		doexe "${FROM}"/libwidevinecdmplugin.so
		doexe "${FROM}"/libwidevinecdm.so
	fi
	exeopts -m4755	# setuid the sandbox
	newexe "${FROM}/chrome_sandbox" chrome-sandbox
	exeopts -m0755

	if use component_build; then
		dodir "${CHROME_DIR}/lib.target"
		exeinto "${CHROME_DIR}/lib.target"
		for f in "${FROM}"/lib.target/*.so; do
			doexe "${f}"
		done
		exeinto "${CHROME_DIR}"
	fi

	# enable the chromeos local account, if the environment dictates
	if [[ -n "${CHROMEOS_LOCAL_ACCOUNT}" ]]; then
		echo "${CHROMEOS_LOCAL_ACCOUNT}" > "${D_CHROME_DIR}/localaccount"
	fi

	# add executable NaCl binaries
	if use_nacl; then
		doexe "${FROM}"/libppGoogleNaClPluginChrome.so || die
		doexe "${FROM}"/nacl_helper_bootstrap || die
	fi

	insinto "${CHROME_DIR}"
	doins "${FROM}"/chrome-wrapper
	doins "${FROM}"/chrome.pak
	doins "${FROM}"/chrome_100_percent.pak
	use content_shell && doins "${FROM}"/content_shell.pak
	doins -r "${FROM}"/locales
	doins -r "${FROM}"/resources
	doins -r "${FROM}"/extensions
	doins "${FROM}"/resources.pak
	doins "${FROM}"/xdg-settings
	doins "${FROM}"/*.png

	# Add high DPI resources.
	if use highdpi; then
		doins "${FROM}"/chrome_200_percent.pak
	fi

	# add non-executable NaCl files
	if use_nacl; then
		doins "${FROM}"/nacl_irt_*.nexe || die
		doins "${FROM}"/nacl_helper || die
	fi

	# Copy input_methods.txt so that ibus-m17n can exclude unnnecessary
	# input methods based on the file.
	insinto /usr/share/chromeos-assets/input_methods
	INPUT_METHOD="${CHROME_ROOT}"/src/chrome/browser/chromeos/input_method
	doins "${INPUT_METHOD}"/input_methods.txt

	# Copy org.chromium.LibCrosService.conf, the D-Bus config file for the
	# D-Bus service exported by Chrome.
	insinto /etc/dbus-1/system.d
	DBUS="${CHROME_ROOT}"/src/chrome/browser/chromeos/dbus
	doins "${DBUS}"/org.chromium.LibCrosService.conf

	# Chrome test resources
	# Test binaries are only available when building chrome from source
	if use build_tests && [[ "${CHROME_ORIGIN}" == "LOCAL_SOURCE" ||
		"${CHROME_ORIGIN}" == "SERVER_SOURCE" ||
		"${CHROME_ORIGIN}" == "GERRIT_SOURCE" ]]; then
		autotest-deponly_src_install
	fi
	if use pgo_generate; then
		local pgo_file_dest="chromeos-chrome-${ARCH}-${PV}${PGO_SUFFIX}"
		echo "${PGO_LOCATION}${pgo_file_dest}" \
			> "${D_CHROME_DIR}/profilelocation"
	fi

	# Fix some perms
	chmod -R a+r "${D}"
	find "${D}" -perm /111 -print0 | xargs -0 chmod a+x

	# The following symlinks are needed in order to run chrome.
	dosym libnss3.so /usr/lib/libnss3.so.1d
	dosym libnssutil3.so.12 /usr/lib/libnssutil3.so.1d
	dosym libsmime3.so.12 /usr/lib/libsmime3.so.1d
	dosym libssl3.so.12 /usr/lib/libssl3.so.1d
	dosym libplds4.so /usr/lib/libplds4.so.0d
	dosym libplc4.so /usr/lib/libplc4.so.0d
	dosym libnspr4.so /usr/lib/libnspr4.so.0d
}
