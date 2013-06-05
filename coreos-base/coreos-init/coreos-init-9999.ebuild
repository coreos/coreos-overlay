# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/init"
CROS_WORKON_LOCALNAME="init"

inherit cros-workon systemd

DESCRIPTION="Init scripts for CoreOS"
HOMEPAGE="http://www.coreos.com/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
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

	# Install all units, enable the higher-level services
	for unit in systemd/*; do
		systemd_dounit "${unit}"
	done

	systemd_enable_service basic.target coreos-startup.service
	systemd_enable_service multi-user.target dhcpcd.service
	systemd_enable_service multi-user.target sshd.socket
	systemd_enable_service multi-user.target update-engine.service
	systemd_enable_service sshd.socket sshd-keygen.service
}
