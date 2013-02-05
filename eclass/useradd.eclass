# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# $Header: $

#
# useradd.eclass
#
# Adds a mechanism for adding users/groups into alternate roots.
#
# This will likely go away.
#
# Authors:
# Google, inc. <chromium-os-dev@chromium.org>
#

HOMEPAGE="http://www.chromium.org/"

# Before we manipulate users at all, we want to make sure that
# passwd/group/shadow is initialized in the first place. That's
# what baselayout does.
if [ "${PN}" != "baselayout" ]; then
	DEPEND="sys-apps/baselayout"
	RDEPEND="sys-apps/baselayout"
fi

# Tests if the user already exists in the passwd file.
#
# $1 - Username (e.g. "messagebus")
user_exists() {
	grep -e "^$1\:" "${ROOT}/etc/passwd" > /dev/null 2>&1
}

# Tests if the group already exists in the group file.
#
# $1 - Groupname (e.g. "messagebus")
group_exists() {
	grep -e "^$1\:" "${ROOT}/etc/group" > /dev/null 2>&1
}

# Add entry to /etc/passwd
#
# $1 - Username (e.g. "messagebus")
# $2 - "*" to indicate not shadowed, "x" to indicate shadowed
# $3 - UID (e.g. 200)
# $4 - GID (e.g. 200)
# $5 - full name (e.g. "")
# $6 - home dir (e.g. "/home/foo" or "/var/run/dbus")
# $7 - shell (e.g. "/bin/sh" or "/bin/false")
add_user() {
	if user_exists "$1"; then
		elog "Skipping add_user of existing user: '$1'"
		return
	fi

	echo "${1}:${2}:${3}:${4}:${5}:${6}:${7}" >> "${ROOT}/etc/passwd"
}

# Remove entry from /etc/passwd
#
# $1 - Username
remove_user() {
	[ -e "${ROOT}/etc/passwd" ] && sed -i -e /^${1}:.\*$/d "${ROOT}/etc/passwd"
}

# Add entry to /etc/shadow
#
# $1 - Username
# $2 - Crypted password
add_shadow() {
	echo "${1}:${2}:14500:0:99999::::" >> "${ROOT}/etc/shadow"
}

# Remove entry from /etc/shadow
#
# $1 - Username
remove_shadow() {
	[ -e "${ROOT}/etc/shadow" ] && sed -i -e /^${1}:.\*$/d "${ROOT}/etc/shadow"
}

# Add entry to /etc/group
# $1 - Groupname (e.g. "messagebus")
# $2 - GID (e.g. 200)
add_group() {
	if group_exists "$1"; then
		elog "Skipping add_group of existing group: '$1'"
		return
	fi
	echo "${1}:x:${2}:" >> "${ROOT}/etc/group"
}

# Copies user entry from host passwd file if it already exists or else
# creates a new user using add_user.
#
# See add_user for argument list.
copy_or_add_user() {
	local username="$1"

	if user_exists "$1"; then
		elog "Skipping copy_or_add_user of existing user '$1'"
		return
	fi

	local entry=$(grep -e "^$1\:" /etc/passwd)
	if [ -n "$entry" ]; then
		elog "Copying existing passwd entry from root: '$entry'"
		echo "$entry" >> "${ROOT}/etc/passwd"
	else
		add_user "$@"
	fi
}

# Copies group entry from host group file if it already exists or else
# creates a new group using add_group.
#
# See add_group for argument list.
copy_or_add_group() {
	local groupname="$1"

	if group_exists "$1"; then
		elog "Skipping copy_or_add_group of existing group '$1'"
		return
	fi

	local entry=$(grep -e "^$1\:" /etc/group)
	if [ -n "$entry" ]; then
		elog "Copying existing group entry from root: '$entry'"
		echo "$entry" >> "${ROOT}/etc/group"
	else
		add_group "$@"
	fi
}
