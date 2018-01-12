# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-libs/libsemanage/libsemanage-2.4-r1.ebuild,v 1.2 2015/05/10 09:02:13 perfinion Exp $

EAPI="5"
PYTHON_COMPAT=( python2_7 python3_3 python3_4 )

inherit multilib python-r1 toolchain-funcs eutils multilib-minimal systemd

MY_P="${P//_/-}"

SEPOL_VER="${PV}"
SELNX_VER="${PV}"

DESCRIPTION="SELinux kernel and policy management library"
HOMEPAGE="https://github.com/SELinuxProject/selinux/wiki"
SRC_URI="https://raw.githubusercontent.com/wiki/SELinuxProject/selinux/files/releases/20150202/${MY_P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86 arm64 arm"
IUSE="python"

RDEPEND=">=sys-libs/libsepol-${SEPOL_VER}[${MULTILIB_USEDEP}]
	>=sys-libs/libselinux-${SELNX_VER}[${MULTILIB_USEDEP}]
	>=sys-process/audit-2.2.2[${MULTILIB_USEDEP}]
	>=dev-libs/ustr-1.0.4-r2[${MULTILIB_USEDEP}]
	"
DEPEND="${RDEPEND}
	sys-devel/bison
	sys-devel/flex
	python? (
		>=dev-lang/swig-2.0.4-r1
		virtual/pkgconfig
		${PYTHON_DEPS}
	)"

# tests are not meant to be run outside of the
# full SELinux userland repo
RESTRICT="test"

S="${WORKDIR}/${MY_P}"

src_prepare() {
	echo "# Set this to true to save the linked policy." >> "${S}/src/semanage.conf"
	echo "# This is normally only useful for analysis" >> "${S}/src/semanage.conf"
	echo "# or debugging of policy." >> "${S}/src/semanage.conf"
	echo "save-linked=false" >> "${S}/src/semanage.conf"
	echo >> "${S}/src/semanage.conf"
	echo "# Set this to 0 to disable assertion checking." >> "${S}/src/semanage.conf"
	echo "# This should speed up building the kernel policy" >> "${S}/src/semanage.conf"
	echo "# from policy modules, but may leave you open to" >> "${S}/src/semanage.conf"
	echo "# dangerous rules which assertion checking" >> "${S}/src/semanage.conf"
	echo "# would catch." >> "${S}/src/semanage.conf"
	echo "expand-check=1" >> "${S}/src/semanage.conf"
	echo >> "${S}/src/semanage.conf"
	echo "# Modules in the module store can be compressed" >> "${S}/src/semanage.conf"
	echo "# with bzip2.  Set this to the bzip2 blocksize" >> "${S}/src/semanage.conf"
	echo "# 1-9 when compressing.  The higher the number," >> "${S}/src/semanage.conf"
	echo "# the more memory is traded off for disk space." >> "${S}/src/semanage.conf"
	echo "# Set to 0 to disable bzip2 compression." >> "${S}/src/semanage.conf"
	echo "bzip-blocksize=0" >> "${S}/src/semanage.conf"
	echo >> "${S}/src/semanage.conf"
	echo "# Reduce memory usage for bzip2 compression and" >> "${S}/src/semanage.conf"
	echo "# decompression of modules in the module store." >> "${S}/src/semanage.conf"
	echo "bzip-small=true" >> "${S}/src/semanage.conf"
	echo "handle-unknown=allow" >> "${S}/src/semanage.conf"

	epatch "${FILESDIR}/0001-libsemanage-do-not-copy-contexts-in-semanage_migrate.patch"

	epatch_user

	multilib_copy_sources
}

multilib_src_compile() {
	emake \
		AR="$(tc-getAR)" \
		CC="$(tc-getCC)" \
		INCLUDEDIR="${ROOT:-/}usr/include" \
		LIBDIR="${EPREFIX}/usr/$(get_libdir)" \
		all

	if multilib_is_native_abi && use python; then
		building_py() {
			python_export PYTHON_INCLUDEDIR PYTHON_LIBPATH
			emake CC="$(tc-getCC)" PYINC="-I${PYTHON_INCLUDEDIR}" PYTHONLBIDIR="${PYTHON_LIBPATH}" PYPREFIX="${EPYTHON##*/}" "$@"
		}
		python_foreach_impl building_py swigify
		python_foreach_impl building_py pywrap
	fi
}

multilib_src_install() {
	emake \
		DEFAULT_SEMANAGE_CONF_LOCATION="${ED}/usr/lib/selinux/semanage.conf" \
		LIBDIR="${ED}/usr/$(get_libdir)" \
		SHLIBDIR="${ED}/usr/$(get_libdir)" \
		DESTDIR="${ED}" install

	if multilib_is_native_abi && use python; then
		installation_py() {
			emake DESTDIR="${ED}" LIBDIR="${ED}/usr/$(get_libdir)" \
				SHLIBDIR="${ED}/usr/$(get_libdir)" install-pywrap
			python_optimize # bug 531638
		}
		python_foreach_impl installation_py
	fi
	systemd_dotmpfilesd "${FILESDIR}/tmpfiles.d/libsemanage.conf"
}
