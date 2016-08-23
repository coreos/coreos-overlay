# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=5

AUTOTOOLS_AUTORECONF=1
AUTOTOOLS_IN_SOURCE_BUILD=1

inherit autotools-utils flag-o-matic git-2 multilib toolchain-funcs

DESCRIPTION="VMware tools for distribution via /usr/share/oem"
HOMEPAGE="http://open-vm-tools.sourceforge.net/"

EGIT_REPO_URI="https://github.com/vmware/open-vm-tools"
EGIT_COMMIT="7bac35f4e8787553f68972feae7966b0236c06ac"
EGIT_SOURCEDIR="${WORKDIR}"

LICENSE="LGPL-2"
SLOT="0"
KEYWORDS="amd64 ~x86"
IUSE="+dnet +pic +deploypkg +vgauth" # TODO: pam

COMMON_DEPEND="
	vgauth? (
		dev-libs/openssl:0
		dev-libs/xerces-c
		dev-libs/xml-security-c
	)"

DEPEND="${COMMON_DEPEND}
	dev-libs/glib:2
	sys-process/procps
	deploypkg? ( dev-libs/libmspack )
	dnet? ( dev-libs/libdnet )"

# Runtime dependencies provided by CoreOS, not the OEM:
#	dev-libs/glib:2
#	sys-apps/ethtool
#	sys-process/procps
#	pam? ( virtual/pam )
RDEPEND="${COMMON_DEPEND}
	dnet? ( dev-libs/libdnet )
	deploypkg? ( dev-libs/libmspack )"

S="${WORKDIR}/${PN}"

PATCHES=(
	"${FILESDIR}/${P}-0001-configure-Add-options-for-fuse-and-hgfs.patch"
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
	local oemlib="/usr/share/oem/$(get_libdir)"
	local oeminc="/usr/share/oem/include"

	# set rpath even if oem is in ld.so.conf
	append-ldflags "-Wl,-rpath,${oemlib}"

	# libdnet is installed to /usr/share/oem
	export CUSTOM_DNET_CPPFLAGS="-I${SYSROOT}${oeminc}"
	export CUSTOM_DNET_LIBS="-L${SYSROOT}${oemlib}"
	export CUSTOM_MSPACK_CPPFLAGS="-I${SYSROOT}${oeminc}"
	export CUSTOM_MSPACK_LIBS="-L${SYSROOT}${oemlib}"

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
		$(use_enable deploypkg)
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
		$(use_enable vgauth)
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
