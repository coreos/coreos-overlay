# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit autotools flag-o-matic multilib toolchain-funcs

DESCRIPTION="Opensourced tools for VMware guests"
HOMEPAGE="https://github.com/vmware/open-vm-tools"
MY_P="${P}-8931395"
SRC_URI="https://github.com/vmware/open-vm-tools/releases/download/stable-${PV}/${MY_P}.tar.gz"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="amd64 ~x86"
IUSE="+dnet +pic +deploypkg" # TODO: pam

DEPEND="dev-libs/glib:2
	net-libs/libtirpc
	deploypkg? ( dev-libs/libmspack )
	dnet? ( dev-libs/libdnet )"

# Runtime dependencies provided by CoreOS, not the OEM:
#	dev-libs/glib:2
#	sys-apps/ethtool
#	pam? ( virtual/pam )
RDEPEND="dnet? ( dev-libs/libdnet )
	deploypkg? ( dev-libs/libmspack )"

S="${WORKDIR}/${MY_P}"

PATCHES=(
	"${FILESDIR}/${P}-0001-configure-Add-options-for-fuse-hgfs-and-udev.patch"
)

src_prepare() {
	eapply -p2 "${PATCHES[@]}"
	eapply_user
	eautoreconf
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
	export CUSTOM_DNET_CPPFLAGS="-I=${oeminc}"
	export CUSTOM_DNET_LIBS="-L=${oemlib}"
	export CUSTOM_MSPACK_CPPFLAGS="-I=${oeminc}"
	export CUSTOM_MSPACK_LIBS="-L=${oemlib}"

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
		--without-fuse
		--without-icu
		--without-kernel-modules
		--without-pam
		--without-udev-rules
		--without-x
		--disable-vgauth
		$(use_with dnet)
		$(use_with pic)
	)
	# TODO: $(use_with pam)

	econf "${myeconfargs[@]}"

	# Bugs 260878, 326761
	find ./ -name Makefile | xargs sed -i -e 's/-Werror//g'  || die "sed out Werror failed"
}

src_install() {
	# Relocate event scripts, a symlink will be created by the systemd
	# unit.
	emake DESTDIR="${D}" confdir=/usr/share/oem/vmware-tools install

	rm "${D}"/etc/pam.d/vmtoolsd
	# TODO: pamd_mimic_system vmtoolsd auth account

	# We never bother with i10n on CoreOS
	rm -rf "${D}"/usr/share/open-vm-tools
}
