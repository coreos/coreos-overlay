# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-emulation/xen-tools/xen-tools-4.2.1.ebuild,v 1.2 2013/01/24 08:53:49 idella4 Exp $

EAPI=5

PYTHON_COMPAT=( python{2_6,2_7} )
PYTHON_REQ_USE='xml,threads'

IPXE_TARBALL_URL="http://dev.gentoo.org/~idella4/tarballs/ipxe.tar.gz"
XEN_SEABIOS_URL="http://dev.gentoo.org/~idella4/tarballs/seabios-0-20121121.tar.bz2"

if [[ $PV == *9999 ]]; then
	KEYWORDS=""
	REPO="xen-unstable.hg"
	EHG_REPO_URI="http://xenbits.xensource.com/${REPO}"
	S="${WORKDIR}/${REPO}"
	live_eclass="mercurial"
else
	KEYWORDS="~amd64 ~x86"
	SRC_URI="http://bits.xensource.com/oss-xen/release/${PV}/xen-${PV}.tar.gz
	$IPXE_TARBALL_URL
	$XEN_SEABIOS_URL"
	S="${WORKDIR}/xen-${PV}"
fi
inherit flag-o-matic eutils multilib python-single-r1 toolchain-funcs udev ${live_eclass}

DESCRIPTION="Xend daemon and tools"
HOMEPAGE="http://xen.org/"
DOCS=( README docs/README.xen-bugtool )

LICENSE="GPL-2"
SLOT="0"
# TODO soon;ocaml
IUSE="api custom-cflags debug doc flask hvm qemu ocaml pygrub screen static-libs xend"

REQUIRED_USE="hvm? ( qemu )"

CDEPEND="<dev-libs/yajl-2
	dev-python/lxml[${PYTHON_USEDEP}]
	dev-python/pypam[${PYTHON_USEDEP}]
	dev-python/pyxml[${PYTHON_USEDEP}]
	sys-libs/zlib
	sys-power/iasl
	dev-ml/findlib
	hvm? ( media-libs/libsdl )
	api? ( dev-libs/libxml2 net-misc/curl )
	${PYTHON_DEPS}
	pygrub? ( ${PYTHON_DEPS//${PYTHON_REQ_USE}/ncurses} )"

DEPEND="${CDEPEND}
	sys-devel/bin86
	sys-devel/dev86
	dev-lang/perl
	app-misc/pax-utils
	dev-ml/findlib
	doc? (
		app-doc/doxygen
		dev-tex/latex2html[png,gif]
		media-gfx/transfig
		media-gfx/graphviz
		dev-tex/xcolor
		dev-texlive/texlive-latexextra
		virtual/latex-base
		dev-tex/latexmk
		dev-texlive/texlive-latex
		dev-texlive/texlive-pictures
		dev-texlive/texlive-latexrecommended
	)
	hvm? (
		x11-proto/xproto
		sys-devel/dev86
	)
	"

RDEPEND="${CDEPEND}
	sys-apps/iproute2
	net-misc/bridge-utils
	>=dev-lang/ocaml-3.12.0
	screen? (
		app-misc/screen
		app-admin/logrotate
	)
	virtual/udev"

# hvmloader is used to bootstrap a fully virtualized kernel
# Approved by QA team in bug #144032
QA_WX_LOAD="usr/lib/xen/boot/hvmloader"

RESTRICT="test"

pkg_setup() {
	python-single-r1_pkg_setup
	export "CONFIG_LOMOUNT=y"

	if has_version dev-libs/libgcrypt; then
		export "CONFIG_GCRYPT=y"
	fi

	if use qemu; then
		export "CONFIG_IOEMU=y"
	else
		export "CONFIG_IOEMU=n"
	fi

	if ! use x86 && ! has x86 $(get_all_abis) && use hvm; then
		eerror "HVM (VT-x and AMD-v) cannot be built on this system. An x86 or"
		eerror "an amd64 multilib profile is required. Remove the hvm use flag"
		eerror "to build xen-tools on your current profile."
		die "USE=hvm is unsupported on this system."
	fi

	if [[ -z ${XEN_TARGET_ARCH} ]] ; then
		if use x86 && use amd64; then
			die "Confusion! Both x86 and amd64 are set in your use flags!"
		elif use x86; then
			export XEN_TARGET_ARCH="x86_32"
		elif use amd64 ; then
			export XEN_TARGET_ARCH="x86_64"
		else
			die "Unsupported architecture!"
		fi
	fi

	use api     && export "LIBXENAPI_BINDINGS=y"
	use flask   && export "FLASK_ENABLE=y"
}

src_prepare() {
	sed -e 's/-Wall//' -i Config.mk || die "Couldn't sanitize CFLAGS"

	# Drop .config
	sed -e '/-include $(XEN_ROOT)\/.config/d' -i Config.mk || die "Couldn't drop"

	# Xend
	if ! use xend; then
		sed -e 's:xm xen-bugtool xen-python-path xend:xen-bugtool xen-python-path:' \
			-i tools/misc/Makefile || die "Disabling xend failed"
		sed -e 's:^XEND_INITD:#XEND_INITD:' \
			-i tools/examples/Makefile || die "Disabling xend failed"
	fi
	# if the user *really* wants to use their own custom-cflags, let them
	if use custom-cflags; then
		einfo "User wants their own CFLAGS - removing defaults"

		# try and remove all the default cflags
		find "${S}" \( -name Makefile -o -name Rules.mk -o -name Config.mk \) \
			-exec sed \
				-e 's/CFLAGS\(.*\)=\(.*\)-O3\(.*\)/CFLAGS\1=\2\3/' \
				-e 's/CFLAGS\(.*\)=\(.*\)-march=i686\(.*\)/CFLAGS\1=\2\3/' \
				-e 's/CFLAGS\(.*\)=\(.*\)-fomit-frame-pointer\(.*\)/CFLAGS\1=\2\3/' \
				-e 's/CFLAGS\(.*\)=\(.*\)-g3*\s\(.*\)/CFLAGS\1=\2 \3/' \
				-e 's/CFLAGS\(.*\)=\(.*\)-O2\(.*\)/CFLAGS\1=\2\3/' \
				-i {} + || die "failed to re-set custom-cflags"
	fi

	if ! use pygrub; then
		sed -e '/^SUBDIRS-$(PYTHON_TOOLS) += pygrub$/d' -i tools/Makefile || die
	fi

	# Disable hvm support on systems that don't support x86_32 binaries.
	if ! use hvm; then
		sed -e '/^CONFIG_IOEMU := y$/d' -i config/*.mk || die
		sed -e '/SUBDIRS-$(CONFIG_X86) += firmware/d' -i tools/Makefile || die
	fi

	# Don't bother with qemu, only needed for fully virtualised guests
	if ! use qemu; then
		sed -e "/^CONFIG_IOEMU := y$/d" -i config/*.mk || die
		sed -e "s:install-tools\: tools/ioemu-dir:install-tools\: :g" -i Makefile || die
	fi

	# Fix build for gcc-4.6
	find "${S}" \( -name Makefile -o -name Rules.mk -o -name Config.mk \) \
		-exec sed -e "s:-Werror::g" -i {} + || die "Failed to remove -Werror"

	# Fix texi2html build error with new texi2html
	sed -r -e "s:(texi2html.*) -number:\1:" \
		-i tools/qemu-xen-traditional/Makefile || die

	# Fix network broadcast on bridged networks
	epatch "${FILESDIR}/${PN}-3.4.0-network-bridge-broadcast.patch"

	# Prevent the downloading of ipxe, seabios
	epatch "${FILESDIR}"/${PN/-tools/}-4.2.0-anti-download.patch
	cp "${DISTDIR}"/ipxe.tar.gz tools/firmware/etherboot/ || die
	mv ../seabios-dir-remote tools/firmware/ || die
	pushd tools/firmware/ > /dev/null
	ln -s seabios-dir-remote seabios-dir || die
	popd > /dev/null

	# Fix bridge by idella4, bug #362575
	epatch "${FILESDIR}/${PN}-4.1.1-bridge.patch"

	# Don't build ipxe with pie on hardened, Bug #360805
	if gcc-specs-pie; then
		epatch "${FILESDIR}/ipxe-nopie.patch"
	fi

	# Prevent double stripping of files at install
	epatch "${FILESDIR}"/${PN/-tools/}-4.2.0-nostrip.patch

	# fix jobserver in Makefile
	epatch "${FILESDIR}"/${PN/-tools/}-4.2.0-jserver.patch
}

src_compile() {
	export VARTEXFONTS="${T}/fonts"
	local myopt
	use debug && myopt="${myopt} debug=y"

	use custom-cflags || unset CFLAGS
	if test-flag-CC -fno-strict-overflow; then
		append-flags -fno-strict-overflow
	fi

	unset LDFLAGS
	unset CFLAGS
	emake CC="$(tc-getCC)" LD="$(tc-getLD)" -C tools ${myopt}

	if use doc; then
		sh ./docs/check_pkgs || die "package check failed"
		emake docs
		emake dev-docs
	fi

	emake -C docs man-pages
}

src_install() {
	# Override auto-detection in the build system, bug #382573
	export INITD_DIR=/tmp/init.d
	export CONFIG_LEAF_DIR=../tmp/default

	# Let the build system compile installed Python modules.
	local PYTHONDONTWRITEBYTECODE
	export PYTHONDONTWRITEBYTECODE

	emake DESTDIR="${ED}" DOCDIR="/usr/share/doc/${PF}" \
		XEN_PYTHON_NATIVE_INSTALL=y install-tools

	# Fix the remaining Python shebangs.
	python_fix_shebang "${ED}"

	# Remove RedHat-specific stuff
	rm -rf "${ED}"tmp || die

	# uncomment lines in xl.conf
	sed -e 's:^#autoballoon=1:autoballoon=1:' \
		-e 's:^#lockfile="/var/lock/xl":lockfile="/var/lock/xl":' \
		-e 's:^#vifscript="vif-bridge":vifscript="vif-bridge":' \
		-i tools/examples/xl.conf  || die

	if use doc; then
		emake DESTDIR="${ED}" DOCDIR="/usr/share/doc/${PF}" install-docs

		dohtml -r docs/api/
		docinto pdf
		dodoc ${DOCS[@]}
		[ -d "${ED}"/usr/share/doc/xen ] && mv "${ED}"/usr/share/doc/xen/* "${ED}"/usr/share/doc/${PF}/html
	fi

	rm -rf "${ED}"/usr/share/doc/xen/
	doman docs/man?/*

	if use xend; then
		newinitd "${FILESDIR}"/xend.initd-r2 xend || die "Couldn't install xen.initd"
	fi
	newconfd "${FILESDIR}"/xendomains.confd xendomains
	newconfd "${FILESDIR}"/xenstored.confd xenstored
	newconfd "${FILESDIR}"/xenconsoled.confd xenconsoled
	newinitd "${FILESDIR}"/xendomains.initd-r2 xendomains
	newinitd "${FILESDIR}"/xenstored.initd xenstored
	newinitd "${FILESDIR}"/xenconsoled.initd xenconsoled

	if use screen; then
		cat "${FILESDIR}"/xendomains-screen.confd >> "${ED}"/etc/conf.d/xendomains || die
		cp "${FILESDIR}"/xen-consoles.logrotate "${ED}"/etc/xen/ || die
		keepdir /var/log/xen-consoles
	fi

	# For -static-libs wrt Bug 384355
	if ! use static-libs; then
		rm -f "${ED}"usr/$(get_libdir)/*.a "${ED}"usr/$(get_libdir)/ocaml/*/*.a
	fi

	# xend expects these to exist
	keepdir /var/run/xenstored /var/lib/xenstored /var/xen/dump /var/lib/xen /var/log/xen

	# for xendomains
	keepdir /etc/xen/auto

	# Temp QA workaround
	dodir "$(udev_get_udevdir)"
	mv "${ED}"/etc/udev/* "${ED}/$(udev_get_udevdir)"
	rm -rf "${ED}"/etc/udev

	# Remove files failing QA AFTER emake installs them, avoiding seeking absent files
	find "${ED}" \( -name openbios-sparc32 -o -name openbios-sparc64 \
		-o -name openbios-ppc -o -name palcode-clipper \) -delete || die
}

pkg_postinst() {
	elog "Official Xen Guide and the unoffical wiki page:"
	elog " http://www.gentoo.org/doc/en/xen-guide.xml"
	elog " http://gentoo-wiki.com/HOWTO_Xen_and_Gentoo"

	if [[ "$(scanelf -s __guard -q "${PYTHON}")" ]] ; then
		echo
		ewarn "xend may not work when python is built with stack smashing protection (ssp)."
		ewarn "If 'xm create' fails with '<ProtocolError for /RPC2: -1 >', see bug #141866"
		ewarn "This problem may be resolved as of Xen 3.0.4, if not post in the bug."
	fi

	# TODO: we need to have the current Python slot here.
	if ! has_version "dev-lang/python[ncurses]"; then
		echo
		ewarn "NB: Your dev-lang/python is built without USE=ncurses."
		ewarn "Please rebuild python with USE=ncurses to make use of xenmon.py."
	fi

	if has_version "sys-apps/iproute2[minimal]"; then
		echo
		ewarn "Your sys-apps/iproute2 is built with USE=minimal. Networking"
		ewarn "will not work until you rebuild iproute2 without USE=minimal."
	fi

	if ! use hvm; then
		echo
		elog "HVM (VT-x and AMD-V) support has been disabled. If you need hvm"
		elog "support enable the hvm use flag."
		elog "An x86 or amd64 multilib system is required to build HVM support."
		echo
		elog "The qemu use flag has been removed and replaced with hvm."
	fi

	if use xend; then
		echo
		elog "xend capability has been enabled and installed"
	fi

	if grep -qsF XENSV= "${ROOT}/etc/conf.d/xend"; then
		echo
		elog "xensv is broken upstream (Gentoo bug #142011)."
		elog "Please remove '${ROOT%/}/etc/conf.d/xend', as it is no longer needed."
	fi
}
