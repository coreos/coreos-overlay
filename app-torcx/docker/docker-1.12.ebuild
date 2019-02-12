# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="Packages to be installed in a torcx image for Docker"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm64"

# Explicitly list all packages that will be built into the image.
RDEPEND="
	~app-emulation/docker-1.12.6
	~app-emulation/containerd-0.2.5
	~app-emulation/runc-1.0.0_rc2_p9
"

src_install() {
	insinto /.torcx
	newins "${FILESDIR}/${P}-manifest.json" manifest.json

	# Enable the Docker socket by default.
	local unitdir=/usr/lib/systemd/system
	dosym ../docker.socket "${unitdir}/sockets.target.wants/docker.socket"
}
