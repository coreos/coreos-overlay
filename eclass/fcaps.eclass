# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/fcaps.eclass,v 1.3 2013/01/30 07:15:49 vapier Exp $

# @ECLASS: fcaps.eclass
# @MAINTAINER:
# Constanze Hausner <constanze@gentoo.org>
# base-system@gentoo.org
# @BLURB: function to set POSIX file-based capabilities
# @DESCRIPTION:
# This eclass provides a function to set file-based capabilities on binaries.
#
# Due to probable capability-loss on moving or copying, this happens in
# pkg_postinst-phase (at least for now).
#
# @EXAMPLE:
# You can manually set the caps on ping and ping6 by doing:
# @CODE
# pkg_postinst() {
# 	fcaps cap_net_raw bin/ping bin/ping6
# }
# @CODE
#
# Or set it via the global ebuild var FILECAPS:
# @CODE
# FILECAPS=(
# 	cap_net_raw bin/ping bin/ping6
# )
# @CODE

if [[ ${___ECLASS_ONCE_FCAPS} != "recur -_+^+_- spank" ]] ; then
___ECLASS_ONCE_FCAPS="recur -_+^+_- spank"

IUSE="+filecaps"

DEPEND="filecaps? ( sys-libs/libcap )"

# @ECLASS-VARIABLE: FILECAPS
# @DEFAULT_UNSET
# @DESCRIPTION:
# An array of fcap arguments to use to automatically execute fcaps.  See that
# function for more details.
#
# All args are consumed until the '--' marker is found.  So if you have:
# @CODE
# 	FILECAPS=( moo cow -- fat cat -- chubby penguin )
# @CODE
#
# This will end up executing:
# @CODE
# 	fcaps moo cow
# 	fcaps fat cat
# 	fcaps chubby penguin
# @CODE
#
# Note: If you override pkg_postinst, you must call fcaps_pkg_postinst yourself.

# @FUNCTION: fcaps
# @USAGE: [-o <owner>] [-g <group>] [-m <mode>] [-M <caps mode>] <capabilities> <file[s]>
# @DESCRIPTION:
# Sets the specified capabilities on the specified files.
#
# The caps option takes the form as expected by the cap_from_text(3) man page.
# If no action is specified, then "=ep" will be used as a default.
#
# If the file is a relative path (e.g. bin/foo rather than /bin/foo), then the
# appropriate path var ($D/$ROOT/etc...) will be prefixed based on the current
# ebuild phase.
#
# The caps mode (default 711) is used to set the permission on the file if
# capabilities were properly set on the file.
#
# If the system is unable to set capabilities, it will use the specified user,
# group, and mode (presumably to make the binary set*id).  The defaults there
# are root:root and 4711.  Otherwise, the ownership and permissions will be
# unchanged.
fcaps() {
	debug-print-function ${FUNCNAME} "$@"

	# Process the user options first.
	local owner='root'
	local group='root'
	local mode='4711'
	local caps_mode='711'

	while [[ $# -gt 0 ]] ; do
		case $1 in
		-o) owner=$2; shift;;
		-g) group=$2; shift;;
		-m) mode=$2; shift;;
		-M) caps_mode=$2; shift;;
		*) break;;
		esac
		shift
	done

	[[ $# -lt 2 ]] && die "${FUNCNAME}: wrong arg count"

	local caps=$1
	[[ ${caps} == *[-=+]* ]] || caps+="=ep"
	shift

	local root
	case ${EBUILD_PHASE} in
	compile|install|preinst)
		root=${ED:-${D}}
		;;
	postinst)
		root=${EROOT:-${ROOT}}
		;;
	esac

	# Process every file!
	local file out
	for file ; do
		[[ ${file} != /* ]] && file="${root}${file}"

		if use filecaps ; then
			# Try to set capabilities.  Ignore errors when the
			# fs doesn't support it, but abort on all others.
			debug-print "${FUNCNAME}: setting caps '${caps}' on '${file}'"

			# If everything goes well, we don't want the file to be readable
			# by people.
			chmod ${caps_mode} "${file}" || die

			if ! out=$(LC_ALL=C setcap "${caps}" "${file}" 2>&1) ; then
				if [[ ${out} != *"Operation not supported"* ]] ; then
					eerror "Setting caps '${caps}' on file '${file}' failed:"
					eerror "${out}"
					die "could not set caps"
				else
					local fstype=$(stat -f -c %T "${file}")
					ewarn "Could not set caps on '${file}' due to missing filesystem support."
					ewarn "Make sure you enable XATTR support for '${fstype}' in your kernel."
					ewarn "You might also have to enable the relevant FS_SECURITY option."
				fi
			else
				# Sanity check that everything took.
				setcap -v "${caps}" "${file}" >/dev/null \
					|| die "Checking caps '${caps}' on '${file}' failed"

				# Everything worked.  Move on to the next file.
				continue
			fi
		fi

		# If we're still here, setcaps failed.
		debug-print "${FUNCNAME}: setting owner/mode on '${file}'"
		chown "${owner}:${group}" "${file}" || die
		chmod ${mode} "${file}" || die
	done
}

# @FUNCTION: fcaps_pkg_postinst
# @DESCRIPTION:
# Process the FILECAPS array.
fcaps_pkg_postinst() {
	local arg args=()
	for arg in "${FILECAPS[@]}" "--" ; do
		if [[ ${arg} == "--" ]] ; then
			fcaps "${args[@]}"
			args=()
		else
			args+=( "${arg}" )
		fi
	done
}

EXPORT_FUNCTIONS pkg_postinst

fi
