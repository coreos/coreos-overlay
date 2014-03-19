# Copyright 2014 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: cros-tmpfiles
# @AUTHOR: marineam
# @BLURB: A basic systemd-tmpfiles --create implementation for ebuilds.
# @DESCRIPTION:
# Any location that is outside of /usr must be initialized during the build
# and (re)created during boot if it is missing. To avoid duplicating
# definitions of these directories/symlinks in ebuilds and tmpfiles configs
# packages can instead only install a tmpfiles config and use this eclass to
# create teh paths in an ebuild friendly way.
#
# Note: in the future if we add a --root option to systemd-tmpfiles we can
# switch to calling that instead of using this simplified implementation.

# Enforce use of recent EAPIs for the sake of consistancy/sanity
case "${EAPI:-0}" in
	0|1|2|3)
		die "Unsupported EAPI=${EAPI:-0} (too old) for ${ECLASS}"
		;;
	4|5)
		;;
	*)
		die "Unsupported EAPI=${EAPI} (unknown) for ${ECLASS}"
		;;
esac

# Since bash doesn't have a slick syntax for subsituting default values
# for anything other than blank vs. non-blank variables this helps.
# Usage: _tmpfiles_set_defaults mode uid gid age arg
_tmpfiles_do_file() {
	[[ ${tmode} == - ]] && tmode=0644
	if [[ "${ttype}" == F ]]; then
		rm -rf "${ED}/${tpath}"
	elif [[ -e "${ED}/${tpath}" ]]; then
		return 0
	fi
	if [[ "${targ}" != - ]]; then
		echo "${targ}" > "${ED}/${tpath}" || return 1
	else
		echo -n > "${ED}/${tpath}" || return 1
	fi
	chmod "${tmode}" "${ED}/${tpath}" || return 1
	chown "${tuid}:${tgid}" "${ED}/${tpath}" || return 1
}

_tmpfiles_do_dir() {
	[[ ${tmode} == - ]] && tmode=0755
	if [[ "${ttype}" == d && -e "${ED}/${tpath}" ]]; then
		return 0
	else
		rm -rf "${ED}/${tpath}"
	fi
	mkdir -m "${tmode}" "${ED}/${tpath}" || return 1
	chown "${tuid}:${tgid}" "${ED}/${tpath}" || return 1
}

_tmpfiles_do_link() {
	if [[ -e "${ED}/${tpath}" || -h "${ED}/${tpath}" ]]; then
		return 0
	fi
	ln -s "${targ}" "${ED}/${tpath}" || return 1
}

_tmpfiles_do_create() {
	local ttype tpath tmode tuid tgid tage targ trule
	while read ttype tpath tmode tuid tgid tage targ; do
		trule="$ttype $tpath $tmode $tuid $tgid $tage $targ"
		[[ "${tuid}" == - ]] && tuid=root
		[[ "${tgid}" == - ]] && tgid=root
		case "${ttype}" in
			f|F)	_tmpfiles_do_file;;
			d|D)	_tmpfiles_do_dir;;
			L)		_tmpfiles_do_link;;
			*)		ewarn "Skipping tmpfiles rule: ${trule}";;
		esac
		if [[ $? -ne 0 ]]; then
			eerror "Bad tmpfiles rule: ${trule}"
			return 1
		fi
	done
}

tmpfiles_create() {
	if [[ $# -eq 0 ]]; then
		set -- "${ED}"/usr/lib*/tmpfiles.d/*.conf
	fi
	local conf
	for conf in "$@"; do
		_tmpfiles_do_create < "${conf}" || die "Bad tmpfiles config: ${conf}"
	done
}
