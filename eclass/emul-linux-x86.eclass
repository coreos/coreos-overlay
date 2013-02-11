# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/emul-linux-x86.eclass,v 1.16 2013/01/12 16:53:56 vapier Exp $

#
# Original Author: Mike Doty <kingtaco@gentoo.org>
# Adapted from emul-libs.eclass
# Purpose: Providing a template for the app-emulation/emul-linux-* packages
#

inherit multilib versionator

if version_is_at_least 20110129; then
	IUSE="development"
else
	IUSE=""
fi

case "${EAPI:-0}" in
	0|1)
		EXPORT_FUNCTIONS src_unpack src_install
		;;
	2|3|4|5)
		EXPORT_FUNCTIONS src_unpack src_prepare src_install
		;;
	*) die "EAPI=${EAPI} is not supported" ;;
esac

if version_is_at_least 20110722; then
	SRC_URI="http://dev.gentoo.org/~pacho/emul/${P}.tar.xz"
else
	if version_is_at_least 20110129; then
		SRC_URI="http://dev.gentoo.org/~pacho/emul/${P}.tar.bz2"
	else
		SRC_URI="mirror://gentoo/${PN}-${PV}.tar.bz2"
	fi
fi

DESCRIPTION="Provides precompiled 32bit libraries"
#HOMEPAGE="http://amd64.gentoo.org/emul/content.xml"
HOMEPAGE="http://dev.gentoo.org/~pacho/emul.html"

RESTRICT="strip"
S=${WORKDIR}

QA_PREBUILT="*"

SLOT="0"

DEPEND=">=sys-apps/findutils-4.2.26"
RDEPEND=""

emul-linux-x86_src_unpack() {
	unpack ${A}
	cd "${S}"
	has ${EAPI:-0} 0 1 && emul-linux-x86_src_prepare
}

emul-linux-x86_src_prepare() {
	ALLOWED=${ALLOWED:-^${S}/etc/env.d}
	has development "${IUSE//+}" && use development && ALLOWED="${ALLOWED}|/usr/lib32/pkgconfig"
	find "${S}" ! -type d ! '(' -name '*.so' -o -name '*.so.[0-9]*' ')' | egrep -v "${ALLOWED}" | xargs -d $'\n' rm -f || die 'failed to remove everything but *.so*'
}

emul-linux-x86_src_install() {
	has ${EAPI:-0} 0 1 2 && ! use prefix && ED="${D}"
	for dir in etc/env.d etc/revdep-rebuild ; do
		if [[ -d "${S}"/${dir} ]] ; then
			for f in "${S}"/${dir}/* ; do
				mv -f "$f"{,-emul}
			done
		fi
	done

	# remove void directories
	find "${S}" -depth -type d -print0 | xargs -0 rmdir 2&>/dev/null

	cp -pPR "${S}"/* "${ED}"/ || die "copying files failed!"

	# Do not hardcode lib32, bug #429726
	local x86_libdir=$(get_abi_LIBDIR x86)
	if [[ ${x86_libdir} != "lib32" ]] ; then
		ewarn "Moving lib32/ to ${x86_libdir}/; some libs might not work"
		mv "${D}"/usr/lib32 "${D}"/usr/${x86_libdir} || die
		if [[ -d ${D}/lib32 ]] ; then
			mv "${D}"/lib32 "${D}"/${x86_libdir} || die
		fi

		pushd "${D}"/usr/${x86_libdir} >/dev/null

		# Fix linker script paths.
		sed -i \
			-e "s:/lib32/:/${x86_libdir}/:" \
			$(grep -ls '^GROUP.*/lib32/' *.so) || die

		# Rewrite symlinks (if need be).
		local sym tgt
		while read sym ; do
			tgt=$(readlink "${sym}")
			ln -sf "${tgt/lib32/${x86_libdir}}" "${sym}" || die
		done < <(find -xtype l)

		popd >/dev/null
	fi
}
