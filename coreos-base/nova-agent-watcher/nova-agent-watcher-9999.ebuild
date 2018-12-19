# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=7
CROS_WORKON_PROJECT="coreos/nova-agent-watcher"
CROS_WORKON_LOCALNAME="nova-agent-watcher"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/nova-agent-watcher"
inherit cros-workon systemd coreos-go

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64  ~arm64"
else
	CROS_WORKON_COMMIT="2262401fe363cfdcc4c6f02144622466d506de43"
	KEYWORDS="amd64 arm64"
fi

DESCRIPTION="nova-agent-watcher"
HOMEPAGE="https://github.com/coreos/nova-agent-watcher"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_install() {
	into "/usr/share/oem"
	dobin ${S}/scripts/gentoo-to-networkd
	dobin ${GOBIN}/nova-agent-watcher
}
