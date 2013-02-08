# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/libxml2/libxml2-2.7.8-r4.ebuild,v 1.6 2012/01/16 02:59:51 jer Exp $

EAPI="3"
PYTHON_DEPEND="python? 2"
PYTHON_USE_WITH="-build xml"
PYTHON_USE_WITH_OPT="python"
SUPPORT_PYTHON_ABIS="1"
RESTRICT_PYTHON_ABIS="3.* *-jython"

inherit libtool flag-o-matic eutils python autotools prefix

DESCRIPTION="Version 2 of the library to manipulate XML files"
HOMEPAGE="http://www.xmlsoft.org/"

LICENSE="MIT"
SLOT="2"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86 ~ppc-aix ~sparc-fbsd ~x86-fbsd ~x64-freebsd ~x86-freebsd ~hppa-hpux ~ia64-hpux ~x86-interix ~amd64-linux ~ia64-linux ~x86-linux ~ppc-macos ~x64-macos ~x86-macos ~m68k-mint ~sparc-solaris ~sparc64-solaris ~x64-solaris ~x86-solaris ~x86-winnt"
IUSE="debug doc examples icu ipv6 python readline static-libs test"

XSTS_HOME="http://www.w3.org/XML/2004/xml-schema-test-suite"
XSTS_NAME_1="xmlschema2002-01-16"
XSTS_NAME_2="xmlschema2004-01-14"
XSTS_TARBALL_1="xsts-2002-01-16.tar.gz"
XSTS_TARBALL_2="xsts-2004-01-14.tar.gz"

SRC_URI="ftp://xmlsoft.org/${PN}/${P}.tar.gz
	test? (
		${XSTS_HOME}/${XSTS_NAME_1}/${XSTS_TARBALL_1}
		${XSTS_HOME}/${XSTS_NAME_2}/${XSTS_TARBALL_2} )"

RDEPEND="sys-libs/zlib
	icu? ( dev-libs/icu )
	readline? ( sys-libs/readline )"

DEPEND="${RDEPEND}
	hppa? ( >=sys-devel/binutils-2.15.92.0.2 )"

pkg_setup() {
	if use python; then
		python_pkg_setup
	fi
}

src_unpack() {
	# ${A} isn't used to avoid unpacking of test tarballs into $WORKDIR,
	# as they are needed as tarballs in ${S}/xstc instead and not unpacked
	unpack ${P}.tar.gz
	cd "${S}"

	if use test; then
		cp "${DISTDIR}/${XSTS_TARBALL_1}" \
			"${DISTDIR}/${XSTS_TARBALL_2}" \
			"${S}"/xstc/ \
			|| die "Failed to install test tarballs"
	fi
}

src_prepare() {
	# Patches needed for prefix support
	epatch "${FILESDIR}"/${PN}-2.7.1-catalog_path.patch
	epatch "${FILESDIR}"/${PN}-2.7.2-winnt.patch

	eprefixify catalog.c xmlcatalog.c runtest.c xmllint.c

	epunt_cxx

	# Reactivate the shared library versionning script
	epatch "${FILESDIR}/${P}-reactivate-script.patch"

	# Fix a potential memory access error
	epatch "${FILESDIR}/${P}-xpath-memory.patch"

	# Fix a potential freeing error in XPath
	epatch "${FILESDIR}/${P}-xpath-freeing.patch"
	epatch "${FILESDIR}/${P}-xpath-freeing2.patch"

	# Fix some potential problems on reallocation failures
	epatch "${FILESDIR}/${P}-reallocation-failures.patch"

	epatch "${FILESDIR}/${P}-disable_static_modules.patch"

	# Hardening of XPath evaluation
	epatch "${FILESDIR}/${P}-hardening-xpath.patch"

	# Fix missing error status in XPath evaluation
	epatch "${FILESDIR}/${P}-error-xpath.patch"

	# Heap-based overflow in parsing long entity references
	epatch "${FILESDIR}/${P}-allocation-error-copying-entities.patch"

	epatch "${FILESDIR}/${P}-stop-parse.patch"
	epatch "${FILESDIR}/${P}-utf-8.patch"

	# Please do not remove, as else we get references to PORTAGE_TMPDIR
	# in /usr/lib/python?.?/site-packages/libxml2mod.la among things.
	# We now need to run eautoreconf at the end to prevent maintainer mode.
#	elibtoolize

	# Python bindings are built/tested/installed manually.
	sed -e "s/@PYTHON_SUBDIR@//" -i Makefile.am || die "sed failed"

	eautoreconf
}

src_configure() {
	# USE zlib support breaks gnome2
	# (libgnomeprint for instance fails to compile with
	# fresh install, and existing) - <azarah@gentoo.org> (22 Dec 2002).

	# The meaning of the 'debug' USE flag does not apply to the --with-debug
	# switch (enabling the libxml2 debug module). See bug #100898.

	# --with-mem-debug causes unusual segmentation faults (bug #105120).

	local myconf="--with-html-subdir=${PF}/html
		--docdir=${EPREFIX}/usr/share/doc/${PF}
		$(use_with debug run-debug)
		$(use_with icu)
		$(use_with python)
		$(use_with readline)
		$(use_with readline history)
		$(use_enable ipv6)
		$(use_enable static-libs static)"

	# filter seemingly problematic CFLAGS (#26320)
	filter-flags -fprefetch-loop-arrays -funroll-loops

	econf ${myconf}
}

src_compile() {
	default

	if use python; then
		python_copy_sources python
		building() {
			emake PYTHON_INCLUDES="${EPREFIX}$(python_get_includedir)" \
				PYTHON_SITE_PACKAGES="${EPREFIX}$(python_get_sitedir)"
		}
		python_execute_function -s --source-dir python building
	fi
}

src_test() {
	default

	if use python; then
		testing() {
			emake test
		}
		python_execute_function -s --source-dir python testing
	fi
}

src_install() {
	emake DESTDIR="${D}" \
		EXAMPLES_DIR="${EPREFIX}"/usr/share/doc/${PF}/examples \
		install || die "Installation failed"

	# on windows, xmllint is installed by interix libxml2 in parent prefix.
	# this is the version to use. the native winnt version does not support
	# symlinks, which makes repoman fail if the portage tree is linked in
	# from another location (which is my default). -- mduft
	if [[ ${CHOST} == *-winnt* ]]; then
		rm -rf "${ED}"/usr/bin/xmllint
		rm -rf "${ED}"/usr/bin/xmlcatalog
	fi

	if use python; then
		installation() {
			emake DESTDIR="${D}" \
				PYTHON_SITE_PACKAGES="${EPREFIX}$(python_get_sitedir)" \
				docsdir="${EPREFIX}"/usr/share/doc/${PF}/python \
				exampledir="${EPREFIX}"/usr/share/doc/${PF}/python/examples \
				install
		}
		python_execute_function -s --source-dir python installation

		python_clean_installation_image
	fi

	rm -rf "${ED}"/usr/share/doc/${P}
	dodoc AUTHORS ChangeLog Copyright NEWS README* TODO* || die "dodoc failed"

	if ! use python; then
		rm -rf "${ED}"/usr/share/doc/${PF}/python
		rm -rf "${ED}"/usr/share/doc/${PN}-python-${PV}
	fi

	if ! use doc; then
		rm -rf "${ED}"/usr/share/gtk-doc
		rm -rf "${ED}"/usr/share/doc/${PF}/html
	fi

	if ! use examples; then
		rm -rf "${ED}/usr/share/doc/${PF}/examples"
		rm -rf "${ED}/usr/share/doc/${PF}/python/examples"
	fi

	if ! use static-libs; then
		# Remove useless .la files
		find "${D}" -name '*.la' -exec rm -f {} + || die "la file removal failed"
	fi
}

pkg_postinst() {
	if use python; then
		python_mod_optimize drv_libxml2.py libxml2.py
	fi

	# We don't want to do the xmlcatalog during stage1, as xmlcatalog will not
	# be in / and stage1 builds to ROOT=/tmp/stage1root. This fixes bug #208887.
	if [ "${ROOT}" != "/" ]
	then
		elog "Skipping XML catalog creation for stage building (bug #208887)."
	else
		# need an XML catalog, so no-one writes to a non-existent one
		CATALOG="${EROOT}etc/xml/catalog"

		# we dont want to clobber an existing catalog though,
		# only ensure that one is there
		# <obz@gentoo.org>
		if [ ! -e ${CATALOG} ]; then
			[ -d "${EROOT}etc/xml" ] || mkdir -p "${EROOT}etc/xml"
			"${EPREFIX}"/usr/bin/xmlcatalog --create > ${CATALOG}
			einfo "Created XML catalog in ${CATALOG}"
		fi
	fi
}

pkg_postrm() {
	if use python; then
		python_mod_cleanup drv_libxml2.py libxml2.py
	fi
}
