# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="bd43c06a3b0eb14ed6d5f93af2a9dfafe060d8bc"
CROS_WORKON_PROJECT="coreos/init"
CROS_WORKON_LOCALNAME="init"

inherit cros-workon systemd

DESCRIPTION="Init scripts for CoreOS"
HOMEPAGE="http://www.coreos.com/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="nfs"

# Daemons we enable here must installed during build/install in addition to
# during runtime so the systemd unit enable step works.
DEPEND="
	net-misc/dhcpcd
	net-misc/openssh
	"
RDEPEND="${DEPEND}
	sys-block/parted
	sys-apps/gptfdisk
	sys-apps/systemd
	"

src_install() {
	# Install our boot scripts along side systemd in /usr/lib
	exeinto /usr/lib/coreos
	for script in scripts/*; do
		doexe "${script}"
	done

	# Install our custom ssh config settings.
	insinto /etc/ssh
	doins configs/ssh{,d}_config
	fperms 600 /etc/ssh/sshd_config

	# List of directories that should be recreated as needed
	insinto /usr/lib/tmpfiles.d
	newins configs/tmpfiles.conf zz-${PN}.conf

	# Install all units, enable the higher-level services
	for unit in systemd/*; do
		systemd_dounit "${unit}"
	done

	# Set the default target to multi-user not graphical, this is CoreOS!
	dosym /usr/lib/systemd/system/multi-user.target /etc/systemd/system/default.target

	systemd_enable_service basic.target coreos-startup.target

	# Services!
	systemd_enable_service default.target local-enable.service
	systemd_enable_service default.target dhcpcd.service
	systemd_enable_service default.target sshd-keygen.service
	systemd_enable_service default.target sshd.socket
}
