# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Install binary packages for Chromium OS
#

# @ECLASS-VARIABLE: CROS_BINARY_STORE_DIR
# @DESCRIPTION:
# Storage directory for Chrome OS Binaries
: ${CROS_BINARY_STORE_DIR:=${PORTAGE_ACTUAL_DISTDIR:-${DISTDIR}}/cros-binary}

# @ECLASS-VARIABLE: CROS_BINARY_FETCH_REQUIRED
# @DESCRIPTION:
# Internal variable controlling whether cros-binary_fetch is actually ran.
: ${CROS_BINARY_FETCH_REQUIRED:=true}

# @ECLASS-FUNCTION: cros-binary_add_uri
# @DESCRIPTION:
# Add a fetch uri to SRC_URI for the given uri.  See
# CROS_BINARY_URI for what is accepted.  Note you cannot
# intermix a non-rewritten ssh w/ (http|https|gs).
cros-binary_add_uri()
{
	if [[ $# -ne 1 ]]; then
		die "cros-binary_add_uri takes exactly one argument; $# given."
	fi
	local uri="$1"
	if [[ "${uri}" =~ ^ssh://([^@]+)@git.chromium.org[^/]*/(.*)$ ]]; then
		uri="gs://chromeos-binaries/HOME/${BASH_REMATCH[1]}/${BASH_REMATCH[2]}"
	fi
	case "${uri}" in
		http://*|https://*|gs://*)
			SRC_URI+=" ${uri}"
			CROS_BINARY_FETCH_REQUIRED=false
			CROS_BINARY_STORE_DIR="${DISTDIR}"
			;;
		*)
			die "Unknown protocol: ${uri}"
			;;
	esac
	RESTRICT+="mirror"
}

# @ECLASS-FUNCTION: cros-binary_add_gs_uri
# @DESCRIPTION:
# Wrapper around cros-binary_add_uri.  Invoked with 3 arguments;
# the bcs user, the overlay, and the filename (or bcs://<uri> for
# backwards compatibility).
cros-binary_add_gs_uri() {
	if [[ $# -ne 3 ]]; then
		die "cros-binary_add_bcs_uri needs 3 arguments; $# given."
	fi
	# Strip leading bcs://...
	[[ "${3:0:6}" == "bcs://" ]] && set -- "${1}" "${2}" "${3#bcs://}"
	cros-binary_add_uri "gs://chromeos-binaries/HOME/$1/$2/$3"
}

# @ECLASS-FUNCTION: cros-binary_add_overlay_uri
# @DESCRIPTION:
# Wrapper around cros-binary_add_bcs_uri.  Invoked with 2 arguments;
# the basic board target (x86-alex for example), and the filename; that filename
# is automatically prefixed with "${CATEGORY}/${PN}/" .
cros-binary_add_overlay_uri() {
	if [[ $# -ne 2 ]]; then
		die "cros-binary_add_bcs_uri_simple needs 2 arguments; $# given."
	fi
	cros-binary_add_gs_uri bcs-"$1" overlay-"$1" "${CATEGORY}/${PN}/$2"
}

# @ECLASS-VARIABLE: CROS_BINARY_URI
# @DESCRIPTION:
# URI for the binary may be one of:
#   http://
#   https://
#   ssh://
#   gs://
#   file:// (file is relative to the files directory)
# Additionally, all bcs ssh:// urls are rewritten to gs:// automatically
# the appropriate GS bucket- although cros-binary_add_uri is the preferred
# way to do that.
# TODO: Deprecate this variable's support for ssh and http/https.
: ${CROS_BINARY_URI:=}
if [[ -n "${CROS_BINARY_URI}" ]]; then
	cros-binary_add_uri "${CROS_BINARY_URI}"
fi

# @ECLASS-VARIABLE: CROS_BINARY_SUM
# @DESCRIPTION:
# Optional SHA-1 sum of the file to be fetched
: ${CROS_BINARY_SUM:=}

# @ECLASS-VARIABLE: CROS_BINARY_INSTALL_FLAGS
# @DESCRIPTION:
# Optional Flags to use while installing the binary
: ${CROS_BINARY_INSTALL_FLAGS:=}

# @ECLASS-VARIABLE: CROS_BINARY_LOCAL_URI_BASE
# @DESCRIPTION:
# Optional URI to override CROS_BINARY_URI location.  If this variable
# is used the filename from CROS_BINARY_URI will be used, but the path
# to the binary will be changed.
: ${CROS_BINARY_LOCAL_URI_BASE:=}

# Check for EAPI 2+
case "${EAPI:-0}" in
	4|3|2) ;;
	*) die "unsupported EAPI" ;;
esac

cros-binary_check_file() {
	local target="${CROS_BINARY_STORE_DIR}/${CROS_BINARY_URI##*/}"
	elog "Checking for cached $target"
	if [[ -z "${CROS_BINARY_SUM}" ]]; then
		return 1
	else
		echo "${CROS_BINARY_SUM}  ${target}" |
			sha1sum -c --quiet >/dev/null 2>&1
		return
	fi
}

cros-binary_fetch() {
	${CROS_BINARY_FETCH_REQUIRED} || return 0
	local uri=${CROS_BINARY_URI}
	if [[ ! -z "${CROS_BINARY_LOCAL_URI_BASE}" ]]; then
		uri="${CROS_BINARY_LOCAL_URI_BASE}/${CROS_BINARY_URI##*/}"
	fi

	local scheme="${uri%%://*}"
	local non_scheme=${uri#*://}
	local netloc=${non_scheme%%/*}
	local server=${netloc%%:*}
	local port=${netloc##*:}
	local path=${non_scheme#*/}

	if [[ -z "${port}" ]]; then
		port="22"
	fi

	local scp_args="-qo BatchMode=yes -oCheckHostIP=no -P ${port}"
	local target="${CROS_BINARY_STORE_DIR}/${uri##*/}"

	addwrite "${CROS_BINARY_STORE_DIR}"
	if [[ ! -d "${CROS_BINARY_STORE_DIR}" ]]; then
		mkdir -p "${CROS_BINARY_STORE_DIR}" ||
			die "Failed to mkdir ${CROS_BINARY_STORE_DIR}"
	fi

	if ! cros-binary_check_file; then
		rm -f "${target}"
		case "${scheme}" in
			http|https|ftp)
				wget "${uri}" -O "${target}" -nv -nc ||
					rm -f "${target}"
				;;

			ssh)
				elog "Running: scp ${scp_args} ${server}:/${path} ${target}"
				scp ${scp_args} "${server}:/${path}" "${target}" ||
					rm -f "${target}"
				;;

			file)
				if [[ "${non_scheme:0:1}" == "/" ]]; then
					cp "${non_scheme}" "${target}" || rm -f "${target}"
				else
					cp "${FILESDIR}/${non_scheme}" "${target}" ||
						rm -f "${target}"
				fi
				;;

			*)
				die "Protocol for '${uri}' is unsupported."
				;;
		esac

		# if no checksum, generate a new one
		if [[ -z "${CROS_BINARY_SUM}" ]]; then
			local sha1=( $(sha1sum "${target}") )
			elog "cros-binary ${target} is not checksummed.  Use:"
			elog "CROS_BINARY_SUM=\"${sha1[0]}\""
			CROS_BINARY_SUM="${sha1[0]}"
		fi
	else
		elog "Not downloading, cached copy available in ${target}"
	fi

	cros-binary_check_file || die "Failed to fetch ${uri}."
}

cros-binary_src_unpack() {
	cros-binary_fetch
}

cros-binary_src_install() {
	local target="${CROS_BINARY_URI##*/}"
	if ${CROS_BINARY_FETCH_REQUIRED}; then
		target="${CROS_BINARY_STORE_DIR}/${target}"
	else
		target="${DISTDIR}/${target}"
	fi

	local extension="${CROS_BINARY_URI##*.}"
	local flags

	case "${CROS_BINARY_URI##*.}" in
		gz|tgz) flags="z";;
		bz2|tbz2) flags="j";;
		*) die "Unsupported binary file format ${CROS_BINARY_URI##*.}"
	esac

	cd "${D}" || die
	tar "${flags}xpf" "${target}" ${CROS_BINARY_INSTALL_FLAGS} || die "Failed to unpack"
}

EXPORT_FUNCTIONS src_unpack src_install

