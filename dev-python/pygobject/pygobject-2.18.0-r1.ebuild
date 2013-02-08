# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-python/pygobject/pygobject-2.18.0.ebuild,v 1.10 2009/08/19 16:30:10 jer Exp $

EAPI="3"
GCONF_DEBUG="no"
GNOME2_LA_PUNT="yes"
SUPPORT_PYTHON_ABIS="1"
# pygobject is partially incompatible with Python 3.
# PYTHON_DEPEND="2:2.6 3:3.1"
# RESTRICT_PYTHON_ABIS="2.4 2.5 3.0 *-jython"
PYTHON_DEPEND="2:2.6"
RESTRICT_PYTHON_ABIS="2.4 2.5 3.* *-jython"

# XXX: Is the alternatives stuff needed anymore?
inherit alternatives autotools gnome2 python virtualx

DESCRIPTION="GLib's GObject library bindings for Python"
HOMEPAGE="http://www.pygtk.org/"

LICENSE="LGPL-2.1"
SLOT="2"
KEYWORDS="alpha amd64 arm hppa ia64 ~mips ppc ppc64 s390 sh sparc x86 ~x86-fbsd"
IUSE="doc examples libffi test"

COMMON_DEPEND=">=dev-libs/glib-2.24.0:2
	libffi? ( virtual/libffi )"
DEPEND="${COMMON_DEPEND}
	doc? (
		dev-libs/libxslt
		>=app-text/docbook-xsl-stylesheets-1.70.1 )
	test? (
		media-fonts/font-cursor-misc
		media-fonts/font-misc-misc )
	>=dev-util/pkgconfig-0.12"
RDEPEND="${COMMON_DEPEND}
	!<dev-python/pygtk-2.13"

pkg_setup() {
	DOCS="AUTHORS ChangeLog* NEWS README"
	G2CONF="${G2CONF}
		--disable-dependency-tracking
		$(use_enable doc docs)
		$(use_with libffi ffi)"
}

src_prepare() {
	gnome2_src_prepare

	# Fix FHS compliance, see upstream bug #535524
	epatch "${FILESDIR}/${PN}-2.15.4-fix-codegen-location.patch"

	# Do not build tests if unneeded, bug #226345
	epatch "${FILESDIR}"/${P}-make_check.patch

	# Do not install files twice, bug #279813
	epatch "${FILESDIR}/${P}-automake111.patch"

	# For cross-compilation we need to compile the constant
	# generation for the HOST architecture while using the
	# target headers.
	if tc-is-cross-compiler ; then
		epatch "${FILESDIR}/${P}-cross-generate-constants.patch"
	fi


	# needed to build on a libtool-1 system, bug #255542
	rm m4/lt* m4/libtool.m4 ltmain.sh

	# disable pyc compiling
	ln -sfn $(type -P true) py-compile

	# Workaround upstream Gentoo bug #232820
	find "${S}" -name .elibtoolized -delete
	eautoreconf

	python_copy_sources
}

src_configure() {
	python_execute_function -s gnome2_src_configure
}

src_compile() {
	python_execute_function -d -s
}

# FIXME: With python multiple ABI support, tests return 1 even when they pass
src_test() {
	unset DBUS_SESSION_BUS_ADDRESS

	testing() {
		XDG_CACHE_HOME="${T}/$(PYTHON --ABI)"
		Xemake check PYTHON=$(PYTHON -a)
	}
	python_execute_function -s testing
}

src_install() {
	[[ -z ${ED} ]] && local ED="${D}"
	installation() {
		gnome2_src_install
		mv "${ED}$(python_get_sitedir)/pygtk.py" "${ED}$(python_get_sitedir)/pygtk.py-2.0"
		mv "${ED}$(python_get_sitedir)/pygtk.pth" "${ED}$(python_get_sitedir)/pygtk.pth-2.0"
	}
	python_execute_function -s installation

	python_clean_installation_image

	sed "s:/usr/bin/python:/usr/bin/python2:" \
		-i "${ED}"/usr/bin/pygobject-codegen-2.0 \
		|| die "Fix usage of python interpreter"

	if use examples; then
		insinto /usr/share/doc/${P}
		doins -r examples || die "doins failed"
	fi
}

pkg_postinst() {
	create_symlinks() {
		alternatives_auto_makesym "$(python_get_sitedir)/pygtk.py" pygtk.py-[0-9].[0-9]
		alternatives_auto_makesym "$(python_get_sitedir)/pygtk.pth" pygtk.pth-[0-9].[0-9]
	}
	python_execute_function create_symlinks

	python_mod_optimize gtk-2.0 pygtk.py
}

pkg_postrm() {
	python_mod_cleanup gtk-2.0 pygtk.py

	create_symlinks() {
		alternatives_auto_makesym "$(python_get_sitedir)/pygtk.py" pygtk.py-[0-9].[0-9]
		alternatives_auto_makesym "$(python_get_sitedir)/pygtk.pth" pygtk.pth-[0-9].[0-9]
	}
	python_execute_function create_symlinks
}
