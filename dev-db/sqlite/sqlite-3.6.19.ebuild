# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-db/sqlite/sqlite-3.6.18.ebuild,v 1.2 2009/09/18 11:34:58 arfrever Exp $

EAPI="2"

inherit eutils flag-o-matic multilib versionator

DESCRIPTION="an SQL Database Engine in a C Library"
HOMEPAGE="http://www.sqlite.org/"
DOC_BASE="$(get_version_component_range 1-3)"
DOC_PV="$(replace_all_version_separators _ ${DOC_BASE})"
SRC_URI="http://www.sqlite.org/${P}.tar.gz
	doc? ( http://www.sqlite.org/${PN}_docs_${DOC_PV}.zip )
	!tcl? ( mirror://gentoo/sqlite3.h-${PV}.bz2 )"

LICENSE="as-is"
SLOT="3"
KEYWORDS="~alpha ~amd64 ~arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc ~sparc-fbsd ~x86 ~x86-fbsd"
IUSE="debug doc icu +readline soundex tcl +threadsafe"
RESTRICT="!tcl? ( test )"

RDEPEND="icu? ( dev-libs/icu )
	readline? ( sys-libs/readline )
	tcl? ( dev-lang/tcl )"
DEPEND="${RDEPEND}
	doc? ( app-arch/unzip )"

pkg_setup() {
	if ! use tcl; then
		ewarn "Installation of SQLite with \"tcl\" USE flag enabled provides more (TCL-unrelated) functionality."

		if use icu; then
			ewarn "Support for ICU is enabled only when \"tcl\" USE flag is enabled."
		fi

		ebeep 1
	fi
}

src_prepare() {
	epatch "${FILESDIR}/${PN}-3.6.16-tkt3922.test.patch"

	epunt_cxx
}

src_configure() {
	# Support column metadata, bug #266651
	append-cppflags -DSQLITE_ENABLE_COLUMN_METADATA

	# Support R-trees, bug #257646
	# Avoid "./.libs/libsqlite3.so: undefined reference to `sqlite3RtreeInit'" during non-amalgamation building.
	if use tcl; then
		append-cppflags -DSQLITE_ENABLE_RTREE
	fi

	if use icu && use tcl; then
		append-cppflags -DSQLITE_ENABLE_ICU
		sed -e "s/TLIBS = @LIBS@/& -licui18n -licuuc/" -i Makefile.in || die "sed failed"
	fi

	# Support soundex, bug #143794
	use soundex && append-cppflags -DSQLITE_SOUNDEX

	econf \
		$(use_enable debug) \
		$(use_enable readline) \
		$(use_enable threadsafe) \
		$(use_enable threadsafe cross-thread-connections) \
		$(use_enable tcl)
}

src_compile() {
	use tcl || cp "${WORKDIR}/sqlite3.h-${PV}" sqlite3.h
	emake TCLLIBDIR="/usr/$(get_libdir)/${P}" || die "emake failed"
}

src_test() {
	if [[ "${EUID}" -ne "0" ]]; then
		local test=test
		use debug && test=fulltest
		emake ${test} || die "some test(s) failed"
	else
		ewarn "The userpriv feature must be enabled to run tests."
		eerror "Testsuite will not be run."
	fi

	if ! use tcl; then
		ewarn "You must enable the tcl USE flag if you want to run the testsuite."
		eerror "Testsuite will not be run."
	fi
}

src_install() {
	emake \
		DESTDIR="${D}" \
		TCLLIBDIR="/usr/$(get_libdir)/${P}" \
		install \
		|| die "emake install failed"

	doman sqlite3.1 || die "doman sqlite3.1 failed"

	if use doc; then
		# Naming scheme changes randomly between - and _ in releases
		# http://www.sqlite.org/cvstrac/tktview?tn=3523
		dohtml -r "${WORKDIR}"/${PN}-${DOC_PV}-docs/* || die "dohtml failed"
	fi
}
