# Copyright (c) 2013 The CoreOS Authors. All rights reserved.
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="castle-coreos-init"
CROS_WORKON_LOCALNAME="castle-coreos-init"
CROS_WORKON_REPO="git@github.com:quantum"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="766ee79d3b2a3c6c6dfab827f50d27e5853fe4a3"
	KEYWORDS="amd64 arm x86"
fi

inherit cros-workon systemd

DESCRIPTION="Init scripts for CoreOS to support Castle scenarios"
HOMEPAGE="http://castle.quantum.com/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
IUSE="test symlink-usr"

REQUIRED_USE="symlink-usr"

# Daemons we enable here must installed during build/install in addition to
# during runtime so the systemd unit enable step works.
DEPEND="
	app-emulation/docker
	coreos-base/coreos-init
	net-misc/openssh
	!<dev-db/etcd-0.0.1-r6
	!coreos-base/oem-service
	test? ( dev-lang/python:2.7 )
	"
RDEPEND="${DEPEND}
	>=sys-apps/systemd-207-r5
	>=coreos-base/coreos-cloudinit-0.1.2-r5
	"

src_install() {
	emake DESTDIR="${D}" install
}

