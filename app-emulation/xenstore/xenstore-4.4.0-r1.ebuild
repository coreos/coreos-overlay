# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-emulation/xen-tools/xen-tools-4.4.0.ebuild,v 1.3 2014/04/04 01:06:31 idella4 Exp $

EAPI=5

PYTHON_COMPAT=( python2_7 )

inherit multilib python-any-r1 systemd toolchain-funcs

MY_PV=${PV/_/-}
S="${WORKDIR}/xen-${MY_PV}"

DESCRIPTION="Xen's xenstore client utility"
HOMEPAGE="http://xen.org/"
SRC_URI="http://bits.xensource.com/oss-xen/release/${MY_PV}/xen-${MY_PV}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 ~arm arm64 ~x86"
IUSE=""

DEPEND="${PYTHON_DEPS}"
RDEPEND=""

pkg_setup() {
	python-any-r1_pkg_setup

	if [[ -z ${XEN_TARGET_ARCH} ]] ; then
		if use x86 && use amd64; then
			die "Confusion! Both x86 and amd64 are set in your use flags!"
		elif use x86; then
			export XEN_TARGET_ARCH="x86_32"
		elif use amd64 ; then
			export XEN_TARGET_ARCH="x86_64"
		elif use arm; then
			export XEN_TARGET_ARCH="arm32"
		elif use arm64; then
			export XEN_TARGET_ARCH="arm64"
		else
			die "Unsupported architecture!"
		fi
	fi
}

src_prepare() {
	sed -e '/^CFLAGS += -Werror$/d' tools/xenstore/Makefile || die
	cp "${FILESDIR}"/config.h tools/ || die
	cp "${FILESDIR}"/Tools.mk config/ || die
}

src_configure() {
	: # configured manually
}

src_compile() {
	local opts=(
		V=1
		CC="$(tc-getCC)"
		LD="$(tc-getLD)"
		AR="$(tc-getAR)"
		PYTHON="${PYTHON}"
		RANLIB="$(tc-getRANLIB)"
		libdir="/usr/$(get_libdir)"
		)
	unset LDFLAGS
	unset CFLAGS
	emake "${opts[@]}" -C tools/include all
	emake "${opts[@]}" -C tools/xenstore clients
}

src_install() {
	dolib.so tools/xenstore/libxenstore.so*
	dobin tools/xenstore/xenstore

	systemd_dounit "${FILESDIR}"/proc-xen.mount
	systemd_enable_service local-fs.target proc-xen.mount
}
