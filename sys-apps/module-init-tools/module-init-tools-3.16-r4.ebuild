# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/module-init-tools/module-init-tools-3.16-r1.ebuild,v 1.8 2012/02/04 00:33:38 williamh Exp $

inherit eutils flag-o-matic

DESCRIPTION="tools for managing linux kernel modules"
HOMEPAGE="http://modules.wiki.kernel.org/"
SRC_URI="mirror://kernel/linux/utils/kernel/module-init-tools/${P}.tar.bz2
	mirror://gentoo/${P}-man.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86"
IUSE="static"
RESTRICT="test"

DEPEND="sys-libs/zlib"
RDEPEND="${DEPEND}
	!<sys-apps/baselayout-2.0.1
	!sys-apps/kmod
	!sys-apps/modutils"

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/${PN}-3.16-use-fd-syscall.patch
	touch *.5 *.8 # dont regen manpages
}

src_compile() {
	mkdir build && cd build #290207
	use static && append-ldflags -static
	ECONF_SOURCE=.. \
	econf \
		--prefix=/ \
		--enable-zlib \
		--enable-zlib-dynamic \
		--disable-static-utils
	emake || die
}

src_test() {
	# this manually runs configure and stuff, so ignore it
	./tests/runtests -v || die
}

src_install() {
	emake -C build install DESTDIR="${D}" || die
	dodoc AUTHORS ChangeLog NEWS README TODO

	into /
	newsbin "${FILESDIR}"/update-modules-3.5.sh update-modules || die
	doman "${FILESDIR}"/update-modules.8 || die

	cat <<-EOF > "${T}"/usb-load-ehci-first.conf
	install ohci_hcd /sbin/modprobe ehci_hcd ; /sbin/modprobe --ignore-install ohci_hcd \$CMDLINE_OPTS
	install uhci_hcd /sbin/modprobe ehci_hcd ; /sbin/modprobe --ignore-install uhci_hcd \$CMDLINE_OPTS
	EOF

	insinto /etc/modprobe.d
	doins "${T}"/usb-load-ehci-first.conf || die #260139
}

pkg_postinst() {
	# cheat to keep users happy
	if grep -qs modules-update "${ROOT}"/etc/init.d/modules ; then
		sed -i 's:modules-update:update-modules:' "${ROOT}"/etc/init.d/modules
	fi

	# For files that were upgraded but not renamed via their ebuild to
	# have a proper .conf extension, rename them so etc-update tools can
	# take care of things. #274942
	local i f cfg
	eshopts_push -s nullglob
	for f in "${ROOT}"etc/modprobe.d/* ; do
		# The .conf files need no upgrading unless a non-.conf exists,
		# so skip this until later ...
		[[ ${f} == *.conf ]] && continue
		# If a .conf doesn't exist, then a package needs updating, or
		# the user created it, or it's orphaned.  Either way, we don't
		# really know, so leave it alone.
		[[ ! -f ${f}.conf ]] && continue

		i=0
		while :; do
			cfg=$(printf "%s/._cfg%04d_%s.conf" "${f%/*}" ${i} "${f##*/}")
			[[ ! -e ${cfg} ]] && break
			((i++))
		done
		elog "Updating ${f}; please run 'etc-update'"
		mv "${f}.conf" "${cfg}"
		mv "${f}" "${f}.conf"
	done
	# Whine about any non-.conf files that are left
	for f in "${ROOT}"etc/modprobe.d/* ; do
		[[ ${f} == *.conf ]] && continue
		ewarn "The '${f}' file needs to be upgraded to end with a '.conf'."
		ewarn "Either upgrade the package that owns it, or manually rename it."
	done
	eshopts_pop
}
