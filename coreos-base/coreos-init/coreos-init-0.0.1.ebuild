# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="402cc75e0f5f230f0aa9c83947e9c262ee85cb1f"
CROS_WORKON_PROJECT="coreos/init"
CROS_WORKON_LOCALNAME="init"

inherit cros-workon systemd

DESCRIPTION="Init scripts for CoreOS"
HOMEPAGE="http://www.coreos.com/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="test"

# Daemons we enable here must installed during build/install in addition to
# during runtime so the systemd unit enable step works.
DEPEND="
	net-misc/dhcpcd
	net-misc/openssh
	!<dev-db/etcd-0.0.1-r6
	!coreos-base/oem-service
	test? ( dev-lang/python:2.7 )
	"
RDEPEND="${DEPEND}
	sys-block/parted
	sys-apps/gptfdisk
	sys-apps/systemd
	"

src_install() {
	default

	# Set the default target to multi-user not graphical, this is CoreOS!
	dosym /usr/lib/systemd/system/multi-user.target /etc/systemd/system/default.target

	systemd_enable_service basic.target coreos-startup.target

	# Services!
	systemd_enable_service default.target coreos-c10n.service
	systemd_enable_service default.target local-enable.service
	systemd_enable_service default.target dhcpcd.service
	systemd_enable_service default.target sshd-keygen.service
	systemd_enable_service default.target sshd.socket
	systemd_enable_service default.target ssh-key-proc-cmdline.service
}
