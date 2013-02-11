# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/gnome-python-common.eclass,v 1.17 2012/05/02 18:31:42 jdhore Exp $

# Original Author: Arun Raghavan <ford_prefect@gentoo.org> (based on the
#		   gnome-python-desktop eclass by Jim Ramsay <lack@gentoo.org>)
#
# Purpose: Provides common functionality required for building the gnome-python*
# 		   bindings
#
# Important environment variables:
#
# G_PY_PN: Which gnome-python* package bindings we're working with. Defaults to
#		   gnome-python if unset.
#
# G_PY_BINDINGS: The actual '--enable-<binding>' name, which by default is ${PN}
# 		   excluding the -python at the end. May be overridden if necessary.
#
# EXAMPLES: The set of example files to be installed if the 'examples' USE flag
# 		   is set.
#
# The naming convention for all bindings is as follows:
#	dev-python/<original-${PN}-for-which-this-is-the-binding>-python
#
# So, for example, with the bonobo bindings, the original package is libbonobo
# and the packages is named dev-python/libbonobo-python

SUPPORT_PYTHON_ABIS="1"
RESTRICT_PYTHON_ABIS="3.* *-jython 2.7-pypy-*"

inherit autotools gnome2 python versionator

case "${EAPI:-0}" in
	0|1)
		EXPORT_FUNCTIONS pkg_setup src_unpack src_compile src_install pkg_postinst pkg_postrm
		;;
	*)
		EXPORT_FUNCTIONS pkg_setup src_prepare src_configure src_compile src_install pkg_postinst pkg_postrm
		;;
esac

G_PY_PN=${G_PY_PN:-gnome-python}
G_PY_BINDINGS=${G_PY_BINDINGS:-${PN%-python}}

PVP="$(get_version_component_range 1-2)"
SRC_URI="mirror://gnome/sources/${G_PY_PN}/${PVP}/${G_PY_PN}-${PV}.tar.bz2"
HOMEPAGE="http://pygtk.org/"

RESTRICT="${RESTRICT} test"

GCONF_DEBUG="no"
DOCS="AUTHORS ChangeLog NEWS README"

if [[ ${G_PY_PN} != "gnome-python" ]]; then
	DOCS="${DOCS} MAINTAINERS"
fi

S="${WORKDIR}/${G_PY_PN}-${PV}"

# add blockers, we can probably remove them later on
if [[ ${G_PY_PN} == "gnome-python-extras" ]]; then
	RDEPEND="!<=dev-python/gnome-python-extras-2.19.1-r2"
fi

RDEPEND="${RDEPEND} ~dev-python/${G_PY_PN}-base-${PV}"
DEPEND="${RDEPEND}
	virtual/pkgconfig"

# Enable the required bindings as specified by the G_PY_BINDINGS variable
gnome-python-common_pkg_setup() {
	python_pkg_setup

	G2CONF="${G2CONF} --disable-allbindings"
	for binding in ${G_PY_BINDINGS}; do
		G2CONF="${G2CONF} --enable-${binding}"
	done
}

gnome-python-common_src_unpack() {
	gnome2_src_unpack

	has ${EAPI:-0} 0 1 && gnome-python-common_src_prepare
}

gnome-python-common_src_prepare() {
	gnome2_src_prepare
	python_clean_py-compile_files

	# The .pc file is installed by respective gnome-python*-base package
	sed -i '/^pkgconfig_DATA/d' Makefile.in || die "sed failed"
	sed -i '/^pkgconfigdir/d' Makefile.in || die "sed failed"

	python_copy_sources
}

gnome-python-common_src_configure() {
	python_execute_function -s gnome2_src_configure "$@"
}

gnome-python-common_src_compile() {
	if has ${EAPI:-0} 0 1; then
		gnome-python-common_src_configure "$@"
		building() {
			emake "$@"
		}
		python_execute_function -s building "$@"
	else
		python_src_compile "$@"
	fi
}

gnome-python-common_src_test() {
	if has ${EAPI:-0} 0 1; then
		testing() {
			if emake -j1 -n check &> /dev/null; then
				emake -j1 check "$@"
			elif emake -j1 -n test &> /dev/null; then
				emake -j1 test "$@"
			fi
		}
		python_execute_function -s testing "$@"
	else
		python_src_test "$@"
	fi
}

# Do a regular gnome2 src_install and then install examples if required.
# Set the variable EXAMPLES to provide the set of examples to be installed.
# (to install a directory recursively, specify it with a trailing '/' - for
# example, foo/bar/)
gnome-python-common_src_install() {
	python_execute_function -s gnome2_src_install "$@"
	python_clean_installation_image

	if has examples ${IUSE} && use examples; then
		insinto /usr/share/doc/${PF}/examples

		for example in ${EXAMPLES}; do
			if [[ ${example: -1} = "/" ]]; then
				doins -r ${example}
			else
				doins ${example}
			fi
		done
	fi
}

gnome-python-common_pkg_postinst() {
	python_mod_optimize gtk-2.0
}

gnome-python-common_pkg_postrm() {
	python_mod_cleanup gtk-2.0
}
