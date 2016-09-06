# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

AUTOTOOLS_AUTORECONF=yes
AUTOTOOLS_IN_SOURCE_BUILD=yes

inherit autotools-utils flag-o-matic systemd toolchain-funcs multilib
inherit cros-workon

CROS_WORKON_PROJECT="coreos/rkt"
CROS_WORKON_LOCALNAME="rkt"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == "9999" ]]; then
	KEYWORDS="~amd64"
else
	KEYWORDS="amd64"
	CROS_WORKON_COMMIT="5393f2e99b1ae3d3b6b232bf428dd15a88714663" # v1.8.0
fi

PXE_VERSION="1097.0.0"
PXE_SYSTEMD_VERSION="v229"
PXE_URI="https://alpha.release.core-os.net/amd64-usr/${PXE_VERSION}/coreos_production_pxe_image.cpio.gz"
PXE_FILE="${PN}-pxe-${PXE_VERSION}.img"

SRC_URI="rkt_stage1_coreos? ( $PXE_URI -> $PXE_FILE )"

DESCRIPTION="A CLI for running app containers, and an implementation of the App
Container Spec."
HOMEPAGE="https://github.com/coreos/rkt"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="doc examples +rkt_stage1_coreos +rkt_stage1_fly rkt_stage1_host rkt_stage1_src +actool tpm"
REQUIRED_USE="|| ( rkt_stage1_coreos rkt_stage1_fly rkt_stage1_host rkt_stage1_src )"

COMMON_DEPEND="sys-apps/acl
		tpm? ( app-crypt/trousers )"
DEPEND="|| ( ~dev-lang/go-1.4.3:= >=dev-lang/go-1.5.3:= )
	app-arch/cpio
	sys-fs/squashfs-tools
	dev-perl/Capture-Tiny
	rkt_stage1_src? (
		>=sys-apps/systemd-222
		app-shells/bash
	)
	${COMMON_DEPEND}"
RDEPEND="!app-emulation/rocket
	actool? ( !app-emulation/actool )
	rkt_stage1_host? (
		~sys-apps/systemd-222
		app-shells/bash
	)
	${COMMON_DEPEND}"

BUILDDIR="build-${P}"

STAGE1INSTALLDIR="/usr/$(get_libdir)/rkt/stage1-images"
STAGE1FIRST=""
STAGE1FLAVORS=""

function add_stage1() {
	if [[ ${STAGE1FIRST} == "" ]]; then
		STAGE1FIRST=$1
		STAGE1FLAVORS=$1
	else
		STAGE1FLAVORS="${STAGE1FLAVORS},$1"
	fi
}

src_prepare() {
	# ensure we use a CoreOS PXE image version that matches rkt's expectations.
	local rkt_coreos_version

	rkt_coreos_version=$(awk '/^CCN_IMG_RELEASE/ { print $3 }' stage1/usr_from_coreos/coreos-common.mk)
	if [ "${rkt_coreos_version}" != "${PXE_VERSION}" ]; then
		die "CoreOS versions in ebuild and rkt build scripts are mismatched, expecting ${rkt_coreos_version}!"
	fi

	autotools-utils_src_prepare
}

src_configure() {
	local myeconfargs=()

	if use rkt_stage1_coreos; then
		add_stage1 "coreos"
		myeconfargs+=( --with-coreos-local-pxe-image-path="${DISTDIR}/${PXE_FILE}" )
		myeconfargs+=( --with-coreos-local-pxe-image-systemd-version="${PXE_SYSTEMD_VERSION}" )
	fi
	if use rkt_stage1_fly; then
		add_stage1 "fly"
	fi
	if use rkt_stage1_host; then
		add_stage1 "host"
	fi
	if use rkt_stage1_src; then
		add_stage1 "src"
	fi

	myeconfargs+=( $(use_enable tpm) )

	myeconfargs+=( --with-stage1-flavors="${STAGE1FLAVORS}" )
	myeconfargs+=( --with-stage1-default-location="${STAGE1INSTALLDIR}/stage1-${STAGE1FIRST}.aci" )

	# Go's 6l linker does not support PIE, disable so cgo binaries
	# which use 6l+gcc for linking can be built correctly.
	if gcc-specs-pie; then
		append-ldflags -nopie
	fi

	export CC=$(tc-getCC)
	export CGO_ENABLED=1
	export CGO_CFLAGS="${CFLAGS}"
	export CGO_CPPFLAGS="${CPPFLAGS}"
	export CGO_CXXFLAGS="${CXXFLAGS}"
	export CGO_LDFLAGS="${LDFLAGS}"
	export BUILDDIR
	export V=1

	autotools-utils_src_configure
}

src_install() {
	dodoc README.md
	use doc && dodoc -r Documentation
	use examples && dodoc -r examples
	use actool && dobin "${S}/${BUILDDIR}/bin/actool"

	dobin "${S}/${BUILDDIR}/bin/rkt"

	einfo The following stage1 ACIs have been installed to ${STAGE1INSTALLDIR}:
	insinto ${STAGE1INSTALLDIR}
	for stage1aci in "${S}/${BUILDDIR}"/bin/stage1-*.aci; do
		doins "${stage1aci}"
		einfo $(basename "${stage1aci}")
	done

	# symlink old stage1 aci directory to the new install location
	dosym ../$(get_libdir)/rkt/stage1-images /usr/share/rkt

	systemd_dounit "${S}"/dist/init/systemd/${PN}-gc.service
	systemd_dounit "${S}"/dist/init/systemd/${PN}-gc.timer
	systemd_enable_service multi-user.target ${PN}-gc.timer
	systemd_dounit "${S}"/dist/init/systemd/${PN}-metadata.service
	systemd_dounit "${S}"/dist/init/systemd/${PN}-metadata.socket
	systemd_enable_service sockets.target ${PN}-metadata.socket
	systemd_dotmpfilesd "${S}"/dist/init/systemd/tmpfiles.d/${PN}.conf

	insinto /usr/lib/sysusers.d/
	newins "${FILESDIR}"/sysusers.conf ${PN}.conf
}
