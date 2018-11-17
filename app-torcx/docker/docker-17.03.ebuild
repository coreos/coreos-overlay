# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="Packages to be installed in a torcx image for Docker"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm64"

# Explicitly list all packages that will be built into the image.
RDEPEND="
	~app-emulation/docker-17.03.2
	~app-emulation/containerd-0.2.6
	~app-emulation/docker-proxy-0.8.0_p20161019
	~app-emulation/docker-runc-1.0.0_rc2_p136
	=sys-process/tini-0.13.2
"

src_install() {
	insinto /.torcx
	newins "${FILESDIR}/${P}-manifest.json" manifest.json

	# Enable the Docker socket by default.
	local unitdir=/usr/lib/systemd/system
	dosym ../docker.socket "${unitdir}/sockets.target.wants/docker.socket"
}
