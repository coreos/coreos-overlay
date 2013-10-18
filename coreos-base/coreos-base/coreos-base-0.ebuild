# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

inherit useradd

DESCRIPTION="ChromeOS specific system setup"
HOMEPAGE="http://src.chromium.org/"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_host"

# We need to make sure timezone-data is merged before us.
# See pkg_setup below as well as http://crosbug.com/27413
# and friends.
DEPEND="sys-apps/baselayout
	sys-apps/efunctions
	!<sys-libs/timezone-data-2011d
	!<=app-admin/sudo-1.8.2
	!<sys-apps/mawk-1.3.4
	!<app-shells/bash-4.1
	!<app-shells/dash-0.5.5
	!<net-misc/openssh-5.2_p1-r8
	!cros_host? (
		sys-libs/timezone-data
	)"
RDEPEND="${DEPEND}
	sys-apps/systemd
	"

# no source directory
S="${WORKDIR}"

# Remove entry from /etc/group
#
# $1 - Group name
remove_group() {
	[ -e "${ROOT}/etc/group" ] && sed -i -e /^${1}:.\*$/d "${ROOT}/etc/group"
}

# Adds a "daemon"-type user with no login or shell.
copy_or_add_daemon_user() {
	local username="$1"
	local uid="$2"
	if user_exists "${username}"; then
		elog "Removing existing user '$1' for copy_or_add_daemon_user"
		remove_user "${username}"
	fi
	copy_or_add_user "${username}" "*" $uid $uid "" /dev/null /bin/false

	if group_exists "${username}"; then
		elog "Removing existing group '$1' for copy_or_add_daemon_user"
		elog "Any existing group memberships will be lost"
		remove_group "${username}"
	fi
	copy_or_add_group "${username}" $uid
}

# Removes all users from a group in /etc/group.
# No changes if the group does not exist.
remove_all_users_from_group() {
	local group="$1"
	sed -i "/^${group}:/s/:[^:]*$/:/" "${ROOT}/etc/group"
}

# Removes a list of users from a group in /etc/group.
# No changes if the group does not exist or the user is not in the group.
remove_users_from_group() {
	local group="$1"; shift
	local username
	for username in "$@"; do
		sed -i -r "/^${group}:/{s/([,:])${username}(,|$)/\1/; s/,$//}" \
			"${ROOT}/etc/group"
	done
}

# Adds a list of users to a group in /etc/group.
# No changes if the group does not exist.
add_users_to_group() {
	local group="$1"; shift
	local username
	remove_users_from_group "${group}" "$@"
	for username in "$@"; do
		sed -i "/^${group}:/{ s/$/,${username}/ ; s/:,/:/ }" "${ROOT}/etc/group"
	done
}

pkg_setup() {
	if ! use cros_host ; then
		# The sys-libs/timezone-data package installs a default /etc/localtime
		# file automatically, so scrub that if it's a regular file.
		local etc_tz="${ROOT}etc/localtime"
		[[ -L ${etc_tz} ]] || rm -f "${etc_tz}"
	fi
}

src_install() {
	dodir /usr/lib/sysctl.d
	insinto /usr/lib/sysctl.d
	newins "${FILESDIR}"/sysctl.conf ${PN}.conf

	# Add a /srv directory for mounting into later
	dodir /srv
	keepdir /srv

	# target-specific fun
	if ! use cros_host ; then
		# Make mount work in the way systemd prescribes
		dosym /proc/mounts /etc/mtab

		# Put resolv.conf in /var/run so root can be read-only
		dosym /var/run/resolv.conf /etc/resolv.conf

		# Insert a cool motd ;)
		insinto /etc
		doins "${FILESDIR}"/motd

		# Insert empty fstab
		doins "${FILESDIR}"/fstab

		# Insert glibc's nsswitch.conf since that is installed weirdly
		doins "${FILESDIR}"/nsswitch.conf

		# Insert a mini vimrc to avoid driving everyone insane
		insinto /etc/vim
		doins "${FILESDIR}"/vimrc

		# Symlink /etc/localtime to something on the stateful partition,
		# which we can then change around at runtime.
		dosym /var/lib/timezone/localtime /etc/localtime || die
	fi

	# Add a sudo file for the core use
	if [[ -n ${SHARED_USER_NAME} ]] ; then
		insinto /etc/sudoers.d
		echo "${SHARED_USER_NAME} ALL=(ALL) NOPASSWD: ALL" > 95_core_base
		insopts -m 440
		doins 95_core_base || die
	fi
}

pkg_postinst() {
	local x

	# We explicitly add all of the users needed in the system here. The
	# build of Chromium OS uses a single build chroot environment to build
	# for various targets with distinct ${ROOT}. This causes two problems:
	#   1. The target rootfs needs to have the same UIDs as the build
	#      chroot so that chmod operations work.
	#   2. The portage tools to add a new user in an ebuild don't work when
	#      $ROOT != /
	# We solve this by having baselayout install in both the build and
	# target and pre-create all needed users. In order to support existing
	# build roots we copy over the user entries if they already exist.
	local system_user="core"
	local system_id="1000"
	local system_home="/home/${system_user}"
	# Add a chronos-access group to provide non-chronos users,
	# mostly system daemons running as a non-chronos user, group permissions
	# to access files/directories owned by chronos.
#	local system_access_user="core-access"
#	local system_access_id="1001"

	local crypted_password='*'
	[ -r "${SHARED_USER_PASSWD_FILE}" ] &&
		crypted_password=$(cat "${SHARED_USER_PASSWD_FILE}")
	remove_user "${system_user}"
	add_user "${system_user}" "x" "${system_id}" \
		"${system_id}" "system_user" "${system_home}" /bin/bash
	remove_shadow "${system_user}"
	add_shadow "${system_user}" "${crypted_password}"

	copy_or_add_group "${system_user}" "${system_id}"
#	copy_or_add_daemon_user "${system_access_user}" "${system_access_id}"
	copy_or_add_daemon_user "messagebus" 201  # For dbus
	copy_or_add_daemon_user "syslog" 202      # For rsyslog
	copy_or_add_daemon_user "ntp" 203
	copy_or_add_daemon_user "sshd" 204
#	copy_or_add_daemon_user "polkituser" 206  # For policykit
#	copy_or_add_daemon_user "tss" 207         # For trousers (TSS/TPM)
#	copy_or_add_daemon_user "pkcs11" 208      # For pkcs11 clients
#	copy_or_add_daemon_user "qdlservice" 209  # for QDLService
#	copy_or_add_daemon_user "cromo" 210       # For cromo (modem manager)
#	copy_or_add_daemon_user "cashew" 211      # Deprecated, do not reuse
#	copy_or_add_daemon_user "ipsec" 212       # For strongswan/ipsec VPN
#	copy_or_add_daemon_user "cros-disks" 213  # For cros-disks
#	copy_or_add_daemon_user "tor" 214         # For tor (anonymity service)
#	copy_or_add_daemon_user "tcpdump" 215     # For tcpdump --with-user
#	copy_or_add_daemon_user "debugd" 216      # For debugd
#	copy_or_add_daemon_user "openvpn" 217     # For openvpn
#	copy_or_add_daemon_user "bluetooth" 218   # For bluez
#	copy_or_add_daemon_user "wpa" 219         # For wpa_supplicant
#	copy_or_add_daemon_user "cras" 220        # For cras (audio)
#	copy_or_add_daemon_user "gavd" 221        # For gavd (audio) (deprecated)
#	copy_or_add_daemon_user "input" 222       # For /dev/input/event access
#	copy_or_add_daemon_user "chaps" 223       # For chaps (pkcs11)
	copy_or_add_daemon_user "dhcp" 224        # For dhcpcd (DHCP client)
#	copy_or_add_daemon_user "tpmd" 225        # For tpmd
#	copy_or_add_daemon_user "mtp" 226         # For libmtp
#	copy_or_add_daemon_user "proxystate" 227  # For proxy monitoring
#	copy_or_add_daemon_user "power" 228       # For powerd
#	copy_or_add_daemon_user "watchdog" 229    # For daisydog
#	copy_or_add_daemon_user "devbroker" 230   # For permission_broker
#	copy_or_add_daemon_user "xorg" 231        # For Xorg
	copy_or_add_daemon_user "etcd" 232        # For etcd
	copy_or_add_daemon_user "docker" 233      # For docker
	copy_or_add_daemon_user "tlsdate" 234      # For tlsdate
	copy_or_add_group "systemd-journal" 248   # For journalctl access
	copy_or_add_group "dialout" 249           # For udev rules
#	copy_or_add_daemon_user "ntfs-3g" 300     # For ntfs-3g prcoess
#	copy_or_add_daemon_user "avfs" 301        # For avfs process
#	copy_or_add_daemon_user "fuse-exfat" 302  # For exfat-fuse prcoess
#	copy_or_add_group "serial" 402

	# Give the core user access to some system tools
	add_users_to_group "docker" "${system_user}"
	add_users_to_group "systemd-journal" "${system_user}"
}
