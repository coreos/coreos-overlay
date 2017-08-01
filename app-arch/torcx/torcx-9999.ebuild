# Copyright (c) 2017 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/torcx"
CROS_WORKON_LOCALNAME="torcx"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/torcx"
COREOS_GO_VERSION="go1.8"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="01135a49b29c03bb9a223958332f5e2a937d613d" # v0.1.0
	KEYWORDS="amd64 arm64"
fi

inherit coreos-go cros-workon systemd

DESCRIPTION="torcx is a boot-time addon manager for immutable systems"
HOMEPAGE="https://github.com/coreos/torcx"
LICENSE="Apache-2.0"
SLOT="0"

src_compile() {
	CGO_ENABLED=0 go_export
	${EGO} build -v \
		-p "$(makeopts_jobs)" \
		-ldflags "-X ${COREOS_GO_PACKAGE}/pkg/version.VERSION=${PV}" \
		-o "bin/${ARCH}/torcx" \
		-tags containers_image_openpgp \
		"${COREOS_GO_PACKAGE}"
}

src_install() {
	local generatordir=/usr/lib/systemd/system-generators
	local vendordir=/usr/share/torcx

#	dobin "${S}/bin/${ARCH}/torcx"

	exeinto "${generatordir}"
	newexe "${S}/bin/${ARCH}/torcx" torcx-generator
#	dosym ../../../bin/torcx "${generatordir}/torcx-generator"
	systemd_dounit "${FILESDIR}/torcx.target"

	insinto "${vendordir}/profiles"
	doins "${FILESDIR}/vendor.json"
	dodir "${vendordir}/store"

	# Preserve program paths for torcx packages.
	newbin "${FILESDIR}/compat-wrapper.sh" docker
	for link in {docker-,}{containerd{,-shim},runc} ctr docker-{init,proxy} dockerd tini
	do ln -fns docker "${ED}/usr/bin/${link}"
	done
	exeinto /usr/lib/coreos
	newexe "${FILESDIR}/dockerd-wrapper.sh" dockerd
}
