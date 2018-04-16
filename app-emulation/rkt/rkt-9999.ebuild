# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=5

AUTOTOOLS_AUTORECONF=yes
AUTOTOOLS_IN_SOURCE_BUILD=yes

# temporary downgrade for https://github.com/coreos/bugs/issues/2402
COREOS_GO_VERSION="go1.9"

inherit autotools-utils flag-o-matic systemd toolchain-funcs multilib
inherit cros-workon coreos-go-depend

CROS_WORKON_PROJECT="rkt/rkt"
CROS_WORKON_LOCALNAME="rkt"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == "9999" ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	KEYWORDS="amd64 arm64"
	CROS_WORKON_COMMIT="e04dd994baa1051f1205578d12d69eec83dbb905" # v1.30.0
fi

PXE_VERSION="1478.0.0"
PXE_SYSTEMD_VERSION="v233"
PXE_FILE="${PN}-pxe-${ARCH}-usr-${PXE_VERSION}.img"

PXE_URI_AMD64="https://alpha.release.core-os.net/amd64-usr/${PXE_VERSION}/coreos_production_pxe_image.cpio.gz"
PXE_URI_ARM64="https://alpha.release.core-os.net/arm64-usr/${PXE_VERSION}/coreos_production_pxe_image.cpio.gz"

PXE_FILE_AMD64="${PN}-pxe-amd64-usr-${PXE_VERSION}.img"
PXE_FILE_ARM64="${PN}-pxe-arm64-usr-${PXE_VERSION}.img"

SRC_URI="rkt_stage1_coreos? (
	amd64? ( ${PXE_URI_AMD64} -> ${PXE_FILE_AMD64} )
	arm64? ( ${PXE_URI_ARM64} -> ${PXE_FILE_ARM64} )
)"

DESCRIPTION="A CLI for running app containers, and an implementation of the App
Container Spec."
HOMEPAGE="https://github.com/rkt/rkt"

LICENSE="Apache-2.0"
SLOT="0"
IUSE="doc examples +rkt_stage1_coreos +rkt_stage1_fly rkt_stage1_host rkt_stage1_src tpm"
REQUIRED_USE="|| ( rkt_stage1_coreos rkt_stage1_fly rkt_stage1_host rkt_stage1_src )"

COMMON_DEPEND="sys-apps/acl
		tpm? ( app-crypt/trousers )"
DEPEND="app-arch/cpio
	sys-fs/squashfs-tools
	rkt_stage1_src? (
		>=sys-apps/systemd-222
		app-shells/bash
	)
	${COMMON_DEPEND}"
RDEPEND="!app-emulation/rocket
	rkt_stage1_host? (
		>=sys-apps/systemd-220
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

	go_export
	export BUILDDIR
	export V=1

	autotools-utils_src_configure
}

src_install() {
	dodoc README.md
	use doc && dodoc -r Documentation
	use examples && dodoc -r examples

	dobin "${S}/${BUILDDIR}/target/bin"/rkt

	einfo The following stage1 ACIs have been installed to ${STAGE1INSTALLDIR}:
	insinto ${STAGE1INSTALLDIR}
	for stage1aci in "${S}/${BUILDDIR}/target/bin"/stage1-*.aci; do
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
