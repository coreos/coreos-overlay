# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=5

AUTOTOOLS_AUTORECONF=1
AUTOTOOLS_IN_SOURCE_BUILD=1

inherit autotools-utils versionator toolchain-funcs

MY_PV="$(replace_version_separator 3 '-')"
MY_P="${PN}-${MY_PV}"
# Downloads are organized into versioned directories
MY_DIR="${PN}/$(version_format_string 'stable-$1.$2.x')"

DESCRIPTION="VMware tools for distribution via /usr/share/oem"
HOMEPAGE="http://open-vm-tools.sourceforge.net/"
SRC_URI="mirror://sourceforge/project/${PN}/${MY_DIR}/${MY_P}.tar.gz"

LICENSE="LGPL-2"
SLOT="0"
KEYWORDS="amd64 ~x86"
IUSE="+dnet +pic" # TODO: pam

DEPEND="dev-libs/glib:2
	sys-process/procps
	dnet? ( dev-libs/libdnet )"

# Runtime dependencies provided by CoreOS, not the OEM:
#	dev-libs/glib:2
#	sys-apps/ethtool
#	sys-process/procps
#	pam? ( virtual/pam )
RDEPEND="dnet? ( dev-libs/libdnet )"

S="${WORKDIR}/${MY_P}"

PATCHES=(
	"${FILESDIR}/${MY_P}-0001-configure-Add-options-for-fuse-and-hgfs.patch"
	"${FILESDIR}/${MY_P}-0002-configure-Fix-USE_SLASH_PROC-conditional.patch"
	"${FILESDIR}/${MY_P}-0003-scripts-Remove-ifup.patch"
	"${FILESDIR}/${MY_P}-0004-auth-Read-from-shadow.patch"
	"${FILESDIR}/${MY_P}-0005-define_USE_SLASH_PROC.patch"
)

#pkg_setup() {
#	enewgroup vmware
#}

src_prepare() {
	autotools-utils_src_prepare
}

# Override configure's use of pkg-config to ensure ${SYSROOT} is respected.
override_vmw_check_lib() {
	local lib="$1"
	local var="$2"
	local pkgconfig="$(tc-getPKG_CONFIG)"
	export "CUSTOM_${var}_CPPFLAGS=$(${pkgconfig} --cflags ${lib})"
	export "CUSTOM_${var}_LIBS=$(${pkgconfig} --libs ${lib})"
}

src_configure() {
	# libdnet is installed to /usr/share/oem
	export CUSTOM_DNET_CPPFLAGS="-I${SYSROOT}/usr/share/oem/include"
	export CUSTOM_DNET_LIBS="-L${SYSROOT}/usr/share/oem/lib64"

	# >=sys-process/procps-3.3.2 not handled by configure
	export CUSTOM_PROCPS_NAME=procps
	override_vmw_check_lib libprocps PROCPS

	# for everything else configure is still wrong because it calls
	# pkg-config directly instead of favoring the ${CHOST}-pkg-config
	# wrapper or using the standard autoconf macro.
	override_vmw_check_lib glib-2.0 GLIB2
	override_vmw_check_lib gmodule-2.0 GMODULE
	override_vmw_check_lib gobject-2.0 GOBJECT
	override_vmw_check_lib gthread-2.0 GTHREAD

	local myeconfargs=(
		--prefix=/usr/share/oem
		--disable-docs
		--disable-hgfs-mounter
		--disable-multimon
		--disable-tests
		--with-procps
		--without-fuse
		--without-icu
		--without-kernel-modules
		--without-pam
		--without-x
		$(use_with dnet)
		$(use_with pic)
	)
	# TODO: $(use_with pam)

	econf "${myeconfargs[@]}"

	# Bugs 260878, 326761
	find ./ -name Makefile | xargs sed -i -e 's/-Werror//g'  || die "sed out Werror failed"
}

src_install() {
	# Relocate event scripts, a symlink will be created by cloudinit.
	emake DESTDIR="${D}" confdir=/usr/share/oem/vmware-tools install

	rm "${D}"/etc/pam.d/vmtoolsd
	# TODO: pamd_mimic_system vmtoolsd auth account

	# We never bother with i10n on CoreOS
	rm -rf "${D}"/usr/share/open-vm-tools
}
