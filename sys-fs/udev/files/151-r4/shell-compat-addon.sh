# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# functions that may not be defined, but are used by the udev-start and udev-stop addon
# used by baselayout-1 and openrc before version 0.4.0

cmd_exist()
{
	type "$1" >/dev/null 2>&1
}

# does not exist in baselayout-1, does exist in openrc
if ! cmd_exist yesno; then
	yesno() {
		[ -z "$1" ] && return 1
		case "$1" in
			yes|Yes|YES) return 0 ;;
		esac
		return 1
	}
fi

# does not exist in baselayout-1, does exist in openrc
if ! cmd_exist fstabinfo; then
	fstabinfo() {
		[ "$1" = "--quiet" ] && shift
		local dir="$1"

		# only check RC_USE_FSTAB on baselayout-1
		yesno "${RC_USE_FSTAB}" || return 1

		# check if entry is in /etc/fstab
		local ret=$(gawk 'BEGIN { found="false"; }
				  $1 ~ "^#" { next }
				  $2 == "'$dir'" { found="true"; }
				  END { print found; }
			    ' /etc/fstab)

		"${ret}"
	}
fi


