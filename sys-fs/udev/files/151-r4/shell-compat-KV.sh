# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# provides get_KV and KV_to_int as not all openrc-versions installed via ebuild have these

cmd_exist()
{
	type "$1" >/dev/null 2>&1
}

# does exist in baselayout-1
# does not exist in openrc, but is added by openrc-ebuild since some time
if ! cmd_exist KV_to_int; then
	KV_to_int() {
		[ -z $1 ] && return 1

		local x=${1%%-*}
		local KV_MAJOR=${x%%.*}
		x=${x#*.}
		local KV_MINOR=${x%%.*}
		x=${x#*.}
		local KV_MICRO=${x%%.*}
		local KV_int=$((${KV_MAJOR} * 65536 + ${KV_MINOR} * 256 + ${KV_MICRO} ))

		# We make version 2.2.0 the minimum version we will handle as
		# a sanity check ... if its less, we fail ...
		[ "${KV_int}" -lt 131584 ] && return 1
	
		echo "${KV_int}"
	}
fi

# same as KV_to_int
if ! cmd_exist get_KV; then
	_RC_GET_KV_CACHE=""
	get_KV() {
		[ -z "${_RC_GET_KV_CACHE}" ] \
			&& _RC_GET_KV_CACHE="$(uname -r)"

		echo "$(KV_to_int "${_RC_GET_KV_CACHE}")"

		return $?
	}
fi

