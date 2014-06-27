# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
CROS_WORKON_PROJECT="coreos/baselayout"
CROS_WORKON_LOCALNAME="baselayout"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="fb3ab27fd28ebd8d8fb9515443d9f21d9e4b2249"
	KEYWORDS="amd64 arm x86"
fi

inherit cros-workon cros-tmpfiles eutils multilib systemd

DESCRIPTION="Filesystem baselayout for CoreOS"
HOMEPAGE="http://www.coreos.com/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
IUSE="cros_host symlink-usr"

# This version of baselayout replaces coreos-base
DEPEND="!coreos-base/coreos-base
	!<sys-libs/glibc-2.17-r1
	!<=sys-libs/nss-usrfiles-2.18.1_pre"

# Make sure coreos-init is not installed in the SDK
RDEPEND="${DEPEND}
	sys-apps/efunctions
	cros_host? ( !coreos-base/coreos-init )"

declare -A LIB_SYMS		# list of /lib->lib64 symlinks
declare -A USR_SYMS		# list of /foo->usr/foo symlinks
declare -a BASE_DIRS	# list of absolute paths that should be directories

# Check that a pre-existing symlink is correct
check_sym() {
	local path="$1" value="$2"
	local real_path=$(readlink -f "${ROOT}${path}")
	local real_value=$(readlink -f "${ROOT}${path%/*}/${value}")
	if [[ -e "${read_path}" && "${read_path}" != "${read_value}" ]]; then
		die "${path} is not a symlink to ${value}"
	fi
}

pkg_setup() {
	local libdirs=$(get_all_libdirs) def_libdir=$(get_abi_LIBDIR $DEFAULT_ABI)

	if [[ -z "${libdirs}" || -z "${def_libdir}" ]]; then
		die "your DEFAULT_ABI=$DEFAULT_ABI appears to be invalid"
	fi

	# figure out which paths should be symlinks and which should be directories
	local d
	for d in bin sbin ${libdirs} ; do
		if [[ "${SYMLINK_LIB}" == "yes" && "${d}" == "lib" ]] ; then
			if use symlink-usr; then
				USR_SYMS["/lib"]="usr/${def_libdir}"
			else
				LIB_SYMS["/lib"]="${def_libdir}"
			fi
			LIB_SYMS["/usr/lib"]="${def_libdir}"
			LIB_SYMS["/usr/local/lib"]="${def_libdir}"
		elif use symlink-usr; then
			USR_SYMS["/$d"]="usr/$d"
			BASE_DIRS+=( "/usr/$d" "/usr/local/$d" )
		else
			BASE_DIRS+=( "/$d" "/usr/$d" "/usr/local/$d" )
		fi
	done

	# make sure any pre-existing symlinks map to the expected locations.
	local sym
	for sym in "${!LIB_SYMS[@]}" ; do
		check_sym "${sym}" "${LIB_SYMS[$sym]}"
	done
	if use symlink-usr; then
		for sym in "${!USR_SYMS[@]}" ; do
			check_sym "${sym}" "${USR_SYMS[$sym]}"
		done
	fi
}

src_compile() {
	default

	# generate a tmpfiles.d config to cover our /usr symlinks
	if use symlink-usr; then
		local tmpfiles="${T}/baselayout-usr.conf"
		echo -n > ${tmpfiles} || die
		for sym in "${!USR_SYMS[@]}" ; do
			echo "L	${sym}	-	-	-	-	${USR_SYMS[$sym]}" >> ${tmpfiles}
		done
	fi
}

src_install() {
	# lib symlinks must be in place before make install
	dodir "${BASE_DIRS[@]}"
	local sym
	for sym in "${!LIB_SYMS[@]}" ; do
		dosym "${LIB_SYMS[$sym]}" "${sym}"
	done
	if use symlink-usr; then
		for sym in "${!USR_SYMS[@]}" ; do
			dosym "${USR_SYMS[$sym]}" "${sym}"
		done
	fi

	emake DESTDIR="${D}" install

	if use symlink-usr; then
		systemd_dotmpfilesd "${T}/baselayout-usr.conf"
	fi

	# Fill in all other paths defined in tmpfiles configs
	tmpfiles_create

	doenvd "env.d/99coreos_ldpath"

	# handle multilib paths.  do it here because we want this behavior
	# regardless of the C library that you're using.  we do explicitly
	# list paths which the native ldconfig searches, but this isn't
	# problematic as it doesn't change the resulting ld.so.cache or
	# take longer to generate.  similarly, listing both the native
	# path and the symlinked path doesn't change the resulting cache.
	local libdir ldpaths
	for libdir in $(get_all_libdirs) ; do
		ldpaths+=":/${libdir}:/usr/${libdir}:/usr/local/${libdir}"
	done
	echo "LDPATH='${ldpaths#:}'" >> "${D}"/etc/env.d/00basic || die

	if ! use symlink-usr ; then
		# modprobe uses /lib instead of /usr/lib
		mv "${D}"/usr/lib/modprobe.d "${D}"/lib/modprobe.d || die
	fi

	# For compatibility with older SDKs which use 1000 for the core user.
	fowners -R 500:500 /home/core || die

	if use cros_host; then
		# Provided by vim in the SDK
		rm -r "${D}"/etc/vim || die
	else
		# Don't install /etc/issue since it is handled by coreos-init right now
		rm "${D}"/etc/issue || die
		sed -i -e '/\/etc\/issue/d' \
			"${D}"/usr/lib/tmpfiles.d/baselayout-etc.conf || die

		# Initialize /etc/passwd, group, and friends on boot.
		bash "scripts/coreos-tmpfiles" "${D}" || die
		dosbin "scripts/coreos-tmpfiles"
		systemd_dounit "scripts/coreos-tmpfiles.service"
		systemd_enable_service sysinit.target coreos-tmpfiles.service
	fi
}
