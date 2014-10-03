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
	CROS_WORKON_COMMIT="699dab7d92915e7e88c216d146c5aa85b7d86322"
	KEYWORDS="amd64 arm x86"
fi

inherit cros-workon systemd

DESCRIPTION="Init scripts for CoreOS"
HOMEPAGE="http://www.coreos.com/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
IUSE="test symlink-usr"

REQUIRED_USE="symlink-usr"

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
	emake DESTDIR="${D}" install

	# Enable some sockets that aren't enabled by their own ebuilds.
	systemd_enable_service sockets.target sshd.socket
	systemd_enable_service sockets.target docker.socket
}
