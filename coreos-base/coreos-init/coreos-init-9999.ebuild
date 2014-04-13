# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/init"
CROS_WORKON_LOCALNAME="init"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="c7db6522790cb82789b0b6e579eeb49a17ca61bf"
	KEYWORDS="amd64 arm x86"
fi

inherit cros-workon systemd

DESCRIPTION="Init scripts for CoreOS"
HOMEPAGE="http://www.coreos.com/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
IUSE="test symlink-usr"

# Daemons we enable here must installed during build/install in addition to
# during runtime so the systemd unit enable step works.
DEPEND="
	app-emulation/docker
	net-misc/openssh
	!<dev-db/etcd-0.0.1-r6
	!coreos-base/oem-service
	test? ( dev-lang/python:2.7 )
	"
RDEPEND="${DEPEND}
	sys-block/parted
	sys-apps/gptfdisk
	>=sys-apps/systemd-207-r5
	>=coreos-base/coreos-cloudinit-0.1.2-r5
	"

src_install() {
	if use symlink-usr ; then
		emake DESTDIR="${D}" install-usr
		systemd_enable_service local-fs.target remount-root.service
		systemd_enable_service default.target resize-btrfs.service
	else
		emake DESTDIR="${D}" install
	fi

	systemd_enable_service basic.target coreos-startup.target

	# Services!
	systemd_enable_service default.target local-enable.service
	systemd_enable_service default.target sshd-keygen.service
	systemd_enable_service default.target sshd.socket
	systemd_enable_service default.target ssh-key-proc-cmdline.service
	systemd_enable_service sockets.target docker.socket
}
