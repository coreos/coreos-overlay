# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-devel/gdb/gdb-9999.ebuild,v 1.3 2011/08/23 16:21:56 vapier Exp $

EAPI="3"

inherit flag-o-matic eutils

export CTARGET=${CTARGET:-${CHOST}}
if [[ ${CTARGET} == ${CHOST} ]] ; then
	if [[ ${CATEGORY/cross-} != ${CATEGORY} ]] ; then
		export CTARGET=${CATEGORY/cross-}
	fi
fi
is_cross() { [[ ${CHOST} != ${CTARGET} ]] ; }

RPM=
MY_PV=${PV}
case ${PV} in
*.*.*.*.*.*)
	# fedora version: gdb-6.8.50.20090302-8.fc11.src.rpm
	inherit versionator rpm
	gvcr() { get_version_component_range "$@"; }
	MY_PV=$(gvcr 1-4)
	RPM="${PN}-${MY_PV}-$(gvcr 5).fc$(gvcr 6).src.rpm"
	SRC_URI="mirror://fedora/development/source/SRPMS/${RPM}"
	;;
*.*.50.*)
	# weekly snapshots
	SRC_URI="ftp://sources.redhat.com/pub/gdb/snapshots/current/gdb-weekly-${PV}.tar.bz2"
	;;
7.2 | 9999*)
	# live git tree
	EGIT_REPO_URI="http://git.chromium.org/chromiumos/third_party/gdb.git"
	EGIT_COMMIT=c4aa8d2c86b0fbfd969fd80fbc91727740a2dd27
	inherit git
	SRC_URI=""
	;;
*)
	# Normal upstream release
	SRC_URI="http://ftp.gnu.org/gnu/gdb/${P}.tar.bz2
		ftp://sources.redhat.com/pub/gdb/releases/${P}.tar.bz2"
	;;
esac

PATCH_VER=""
DESCRIPTION="GNU debugger"
HOMEPAGE="http://sourceware.org/gdb/"
SRC_URI="${SRC_URI} ${PATCH_VER:+mirror://gentoo/${P}-patches-${PATCH_VER}.tar.xz}"

LICENSE="GPL-2 LGPL-2"
is_cross \
	&& SLOT="${CTARGET}" \
	|| SLOT="0"
if [[ ${PV} != 9999* ]] ; then
	KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sparc x86 ~x86-fbsd"
fi
IUSE="expat multitarget nls python test vanilla mounted_sources"

RDEPEND=">=sys-libs/ncurses-5.2-r2
	sys-libs/readline
	expat? ( dev-libs/expat )
	python? ( =dev-lang/python-2* )"
DEPEND="${RDEPEND}
	app-arch/xz-utils
	virtual/yacc
	test? ( dev-util/dejagnu )
	nls? ( sys-devel/gettext )"

S=${WORKDIR}/${PN}-${MY_PV}

src_unpack () {
	if use mounted_sources ; then
		: ${GDBDIR:=/usr/local/toolchain_root/gdb/gdb-7.2.x}
		if [[ ! -d ${GDBDIR} ]] ; then
			die "gdb dir not mounted/present at: ${GDBDIR}"
		fi
		cp -R ${GDBDIR} ${S}
	else
		git_src_unpack
	fi
}

src_prepare() {
	[[ -n ${RPM} ]] && rpm_spec_epatch "${WORKDIR}"/gdb.spec
	use vanilla || [[ -n ${PATCH_VER} ]] && EPATCH_SUFFIX="patch" epatch "${WORKDIR}"/patch
	strip-linguas -u bfd/po opcodes/po
}

gdb_branding() {
	printf "Gentoo ${PV} "
	if [[ -n ${PATCH_VER} ]] ; then
		printf "p${PATCH_VER}"
	else
		printf "vanilla"
	fi
}

src_configure() {
	strip-unsupported-flags
	econf \
		--with-pkgversion="$(gdb_branding)" \
		--with-bugurl='http://bugs.gentoo.org/' \
		--disable-werror \
		--enable-64-bit-bfd \
		--with-system-readline \
		--with-separate-debug-dir=/usr/lib/debug \
		$(is_cross && echo --with-sysroot=/usr/${CTARGET}) \
		$(use_with expat) \
		$(use_enable nls) \
		$(use multitarget && echo --enable-targets=all) \
		$(use_with python python "${EPREFIX}/usr/bin/python2")
}

src_test() {
	emake check || ewarn "tests failed"
}

src_install() {
	emake \
		DESTDIR="${D}" \
		{include,lib}dir=/nukeme/pretty/pretty/please \
		install || die
	rm -r "${D}"/nukeme || die

	# Don't install docs when building a cross-gdb
	if [[ ${CTARGET} != ${CHOST} ]] ; then
		rm -r "${D}"/usr/share
		return 0
	fi

	dodoc README
	docinto gdb
	dodoc gdb/CONTRIBUTE gdb/README gdb/MAINTAINERS \
		gdb/NEWS gdb/ChangeLog gdb/PROBLEMS
	docinto sim
	dodoc sim/ChangeLog sim/MAINTAINERS sim/README-HACKING

	if [[ -n ${PATCH_VER} ]] ; then
		dodoc "${WORKDIR}"/extra/gdbinit.sample
	fi

	# Remove shared info pages
	rm -f "${D}"/usr/share/info/{annotate,bfd,configure,standards}.info*
}

pkg_postinst() {
	# portage sucks and doesnt unmerge files in /etc
	rm -vf "${ROOT}"/etc/skel/.gdbinit
}
