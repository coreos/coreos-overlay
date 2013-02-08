# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-python/pygtk/pygtk-2.14.1-r1.ebuild,v 1.8 2009/09/06 20:57:05 ranger Exp $

EAPI="3"
GCONF_DEBUG="no"
PYTHON_DEPEND="2:2.6"
SUPPORT_PYTHON_ABIS="1"
# dev-python/pycairo does not support Python 2.4 / 2.5.
RESTRICT_PYTHON_ABIS="2.4 2.5 3.* *-jython"
PYTHON_EXPORT_PHASE_FUNCTIONS="1"

inherit alternatives autotools eutils flag-o-matic gnome.org python virtualx

DESCRIPTION="GTK+2 bindings for Python"
HOMEPAGE="http://www.pygtk.org/"

LICENSE="LGPL-2.1"
SLOT="2"
KEYWORDS="alpha amd64 arm hppa ia64 ~mips ppc ppc64 sh sparc x86 ~x86-fbsd"
IUSE="doc examples"

RDEPEND=">=dev-libs/glib-2.8:2
	>=x11-libs/pango-1.16
	>=dev-libs/atk-1.12
	>=x11-libs/gtk+-2.13.6
	>=dev-python/pycairo-1.0.2
	>=dev-python/pygobject-2.15.3
	dev-python/numpy
	>=gnome-base/libglade-2.5:2.0
"
DEPEND="${RDEPEND}
	doc? (
		dev-libs/libxslt
		>=app-text/docbook-xsl-stylesheets-1.70.1 )
	>=dev-util/pkgconfig-0.9"

src_prepare() {
	# Fix declaration of codegen in .pc
	epatch "${FILESDIR}/${PN}-2.13.0-fix-codegen-location.patch"

	# Fix test failurs due to ltihooks
	# gentoo bug #268315, upstream bug #565593
	epatch "${FILESDIR}/${P}-ltihooks.patch"

	# Switch to numpy, bug #185692
	epatch "${FILESDIR}/${P}-numpy.patch"
	epatch "${FILESDIR}/${P}-fix-numpy-warning.patch"

	# Fix bug with GtkToggleButton and gtk+-2.16, bug #275449
	epatch "${FILESDIR}/${P}-gtktoggle.patch"

	epatch "${FILESDIR}"/${PN}-2.14.1-libdir-pc.patch

	# Disable pyc compiling
	mv "${S}"/py-compile "${S}"/py-compile.orig
	ln -s $(type -P true) "${S}"/py-compile

	AT_M4DIR="m4" eautoreconf

	python_copy_sources
}

src_configure() {
	use hppa && append-flags -ffunction-sections
	python_src_configure \
		$(use_enable doc docs) \
		--enable-thread
}

src_test() {
	unset DBUS_SESSION_BUS_ADDRESS

	testing() {
		cd tests
		export XDG_CONFIG_HOME="${T}/$(PYTHON --ABI)"
		Xemake check-local
	}
	python_execute_function -s testing
}

src_install() {
	python_src_install
	python_clean_installation_image
	dodoc AUTHORS ChangeLog INSTALL MAPPING NEWS README THREADS TODO || die

	if use examples; then
		rm examples/Makefile*
		insinto /usr/share/doc/${PF}
		doins -r examples || die
	fi
}

pkg_postinst() {
	python_mod_optimize gtk-2.0

	create_symlinks() {
		alternatives_auto_makesym $(python_get_sitedir)/pygtk.py pygtk.py-[0-9].[0-9]
		alternatives_auto_makesym $(python_get_sitedir)/pygtk.pth pygtk.pth-[0-9].[0-9]
	}
	python_execute_function create_symlinks
}

pkg_postrm() {
	python_mod_cleanup gtk-2.0

	create_symlinks() {
		alternatives_auto_makesym $(python_get_sitedir)/pygtk.py pygtk.py-[0-9].[0-9]
		alternatives_auto_makesym $(python_get_sitedir)/pygtk.pth pygtk.pth-[0-9].[0-9]
	}
	python_execute_function create_symlinks
}
