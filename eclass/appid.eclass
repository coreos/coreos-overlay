# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# $Header: $

# @ECLASS: appid.eclass
# @MAINTAINER:
# ChromiumOS Build Team
# @BUGREPORTS:
# Please report bugs via http://crosbug.com/new (with label Area-Build)
# @VCSURL: http://git.chromium.org/gitweb/?p=chromiumos/overlays/chromiumos-overlay.git;a=blob;f=eclass/@ECLASS@
# @BLURB: Eclass for setting up the omaha appid field in /etc/lsb-release

# @FUNCTION: doappid
# @USAGE: <appid>
# @DESCRIPTION:
# Initializes /etc/lsb-release with the appid.  Note that appid is really
# just a UUID in the canonical {8-4-4-4-12} format (all uppercase).  e.g.
# {01234567-89AB-CDEF-0123-456789ABCDEF}
doappid() {
	[[ $# -eq 1 && -n $1 ]] || die "Usage: ${FUNCNAME} <appid>"
	local appid=$1

	# Validate the UUID is formatted correctly.  Except for mario --
	# it was created before we had strict rules, and so it violates :(.
	if [[ ${appid} != '{87efface-864d-49a5-9bb3-4b050a7c227a}' ]] ; then
		local uuid_regex='[{][0-9A-F]{8}-([0-9A-F]{4}-){3}[0-9A-F]{12}[}]'
		local filtered_appid=$(echo "${appid}" | LC_ALL=C sed -r "s:${uuid_regex}::")
		if [[ -n ${filtered_appid} ]] ; then
			eerror "Invalid appid: ${appid} -> ${filtered_appid}"
			eerror "  - must start with '{' and end with '}'"
			eerror "  - must be all upper case"
			eerror "  - be a valid UUID (8-4-4-4-12 hex digits)"
			die "invalid appid: ${appid}"
		fi
	fi

	dodir /etc

	local lsb="${D}/etc/lsb-release"
	[[ -e ${lsb} ]] && die "${lsb} already exists!"
	echo "CHROMEOS_RELEASE_APPID=${appid}" > "${lsb}" || die "creating ${lsb} failed!"
}
