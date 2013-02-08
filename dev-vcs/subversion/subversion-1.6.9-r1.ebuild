# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-util/subversion/subversion-1.6.9.ebuild,v 1.5 2010/03/04 15:28:56 jer Exp $

EAPI="2"

WANT_AUTOMAKE="none"

inherit autotools bash-completion db-use depend.apache elisp-common eutils flag-o-matic java-pkg-opt-2 libtool multilib perl-module python

DESCRIPTION="Advanced version control system"
HOMEPAGE="http://subversion.tigris.org/"
SRC_URI="http://subversion.tigris.org/downloads/${P/_/-}.tar.bz2"

LICENSE="Subversion"
SLOT="0"
KEYWORDS="~alpha amd64 ~arm hppa ~ia64 ~ppc ppc64 ~s390 ~sh ~sparc x86 ~x86-fbsd"
IUSE="apache2 berkdb ctypes-python debug doc +dso emacs extras gnome-keyring java kde nls perl python ruby sasl test vim-syntax +webdav-neon webdav-serf"

CDEPEND=">=dev-db/sqlite-3.4[threadsafe]
	>=dev-libs/apr-1.3:1
	>=dev-libs/apr-util-1.3:1
	dev-libs/expat
	sys-libs/zlib
	berkdb? ( =sys-libs/db-4* )
	emacs? ( virtual/emacs )
	gnome-keyring? ( dev-libs/glib:2 sys-apps/dbus gnome-base/gnome-keyring )
	kde? ( sys-apps/dbus x11-libs/qt-core x11-libs/qt-dbus x11-libs/qt-gui >=kde-base/kdelibs-4 )
	ruby? ( >=dev-lang/ruby-1.8.2 )
	sasl? ( dev-libs/cyrus-sasl )
	dev-util/subversion
	net-misc/neon
	webdav-neon? ( >=net-libs/neon-0.28 )
	webdav-serf? ( >=net-libs/serf-0.3.0 )"
RDEPEND="${CDEPEND}
	apache2? ( www-servers/apache[apache2_modules_dav] )
	java? ( >=virtual/jre-1.5 )
	kde? ( kde-base/kwalletd )
	nls? ( virtual/libintl )
	perl? ( dev-perl/URI )"
APACHE_TEST_DEPEND="|| (
	=www-servers/apache-2.4*[apache2_modules_auth_basic,apache2_modules_authn_core,apache2_modules_authn_file,apache2_modules_authz_core,apache2_modules_authz_user,apache2_modules_dav,apache2_modules_log_config,apache2_modules_unixd]
	=www-servers/apache-2.2*[apache2_modules_auth_basic,apache2_modules_authn_file,apache2_modules_dav,apache2_modules_log_config]
	)"
DEPEND="${CDEPEND}
	>=sys-apps/sandbox-1.6
	ctypes-python? ( dev-python/ctypesgen )
	doc? ( app-doc/doxygen )
	gnome-keyring? ( dev-util/pkgconfig )
	java? ( >=virtual/jdk-1.5 )
	kde? ( dev-util/pkgconfig )
	nls? ( sys-devel/gettext )
	test? (
		webdav-neon? ( ${APACHE_TEST_DEPEND} )
		webdav-serf? ( ${APACHE_TEST_DEPEND} )
	)
	webdav-neon? ( dev-util/pkgconfig )"

want_apache

S="${WORKDIR}/${P/_/-}"

# Allow for custom repository locations.
# This can't be in pkg_setup() because the variable needs to be available to pkg_config().
: ${SVN_REPOS_LOC:=/var/svn}

pkg_setup() {
	if use kde && ! use nls; then
		eerror "Support for KWallet (KDE) requires Native Language Support (NLS)."
		die "Enable \"nls\" USE flag"
	fi

	if use berkdb; then
		einfo
		if [[ -z "${SVN_BDB_VERSION}" ]]; then
			SVN_BDB_VERSION="$(db_ver_to_slot "$(db_findver sys-libs/db 2>/dev/null)")"
			einfo "SVN_BDB_VERSION variable isn't set. You can set it to enforce using of specific version of Berkeley DB."
		fi
		einfo "Using: Berkeley DB ${SVN_BDB_VERSION}"
		einfo

		local apu_bdb_version="$(scanelf -nq "${ROOT}usr/$(get_libdir)/libaprutil-1.so.0" | grep -Eo "libdb-[[:digit:]]+\.[[:digit:]]+" | sed -e "s/libdb-\(.*\)/\1/")"
		if [[ -n "${apu_bdb_version}" && "${SVN_BDB_VERSION}" != "${apu_bdb_version}" ]]; then
			eerror "APR-Util is linked against Berkeley DB ${apu_bdb_version}, but you are trying"
			eerror "to build Subversion with support for Berkeley DB ${SVN_BDB_VERSION}."
			eerror "Rebuild dev-libs/apr-util or set SVN_BDB_VERSION=\"${apu_bdb_version}\"."
			eerror "Aborting to avoid possible run-time crashes."
			die "Berkeley DB version mismatch"
		fi
	fi

	depend.apache_pkg_setup

	java-pkg-opt-2_pkg_setup

	if ! use webdav-neon && ! use webdav-serf; then
		ewarn
		ewarn "WebDAV support is disabled. You need WebDAV to"
		ewarn "access repositories through the HTTP protocol."
		ewarn
		ewarn "WebDAV support needs one of the following USE flags enabled:"
		ewarn "  webdav-neon webdav-serf"
		ewarn
		ewarn "You can do this by enabling one of these flags in /etc/portage/package.use:"
		ewarn "    ${CATEGORY}/${PN} webdav-neon webdav-serf"
		ewarn
		ebeep
	fi

	if use test; then
		elog
		elog "\e[1;31m************************************************************************************************\e[0m"
		elog
		elog "NOTES ABOUT TESTS"
		elog
		elog "You can set the following variables to enable testing of some features and configure testing:"
		if use webdav-neon || use webdav-serf; then
			elog "  SVN_TEST_APACHE_PORT=integer          - Set Apache port number (Default value: 62208)"
		fi
		elog "  SVN_TEST_SVNSERVE_PORT=integer        - Set svnserve port number (Default value: 62209)"
		elog "  SVN_TEST_FSFS_MEMCACHED=1             - Enable using of Memcached for FSFS repositories"
		elog "  SVN_TEST_FSFS_MEMCACHED_PORT=integer  - Set Memcached port number (Default value: 62210)"
		elog "  SVN_TEST_FSFS_SHARDING=integer        - Enable sharding of FSFS repositories and set default shard size for FSFS repositories"
		elog "  SVN_TEST_FSFS_PACKING=1               - Enable packing of FSFS repositories"
		elog "                                          (SVN_TEST_FSFS_PACKING requires SVN_TEST_FSFS_SHARDING)"
#		if use sasl; then
#	 		elog "  SVN_TEST_SASL=1                       - Enable SASL authentication"
#		fi
		if use ctypes-python || use java || use perl || use python || use ruby; then
			elog "  SVN_TEST_BINDINGS=1                   - Enable testing of bindings"
		fi
		if use java || use perl || use python || use ruby; then
			elog "                                          (Testing of bindings requires ${CATEGORY}/${PF})"
		fi
		if use java; then
			elog "                                          (Testing of JavaHL library requires dev-java/junit:4)"
		fi
		elog
		elog "\e[1;31m************************************************************************************************\e[0m"
		elog
		ebeep
		epause 24

		if ! use apache2 && { use webdav-neon || use webdav-serf; }; then
			eerror "Testing of libsvn_ra_neon / libsvn_ra_serf requires support for Apache."
			die "Enable \"apache2\" USE flag."
		fi

		if [[ -n "${SVN_TEST_APACHE_PORT}" ]] && ! ([[ "$((${SVN_TEST_APACHE_PORT}))" == "${SVN_TEST_APACHE_PORT}" ]]) &>/dev/null; then
			die "Value of SVN_TEST_APACHE_PORT must be an integer"
		fi

		if [[ -n "${SVN_TEST_SVNSERVE_PORT}" ]] && ! ([[ "$((${SVN_TEST_SVNSERVE_PORT}))" == "${SVN_TEST_SVNSERVE_PORT}" ]]) &>/dev/null; then
			die "Value of SVN_TEST_SVNSERVE_PORT must be an integer"
		fi

		if [[ -n "${SVN_TEST_FSFS_MEMCACHED}" ]] && ! has_version net-misc/memcached; then
			die "net-misc/memcached must be installed"
		fi
		if [[ -n "${SVN_TEST_FSFS_MEMCACHED_PORT}" ]] && ! ([[ "$((${SVN_TEST_FSFS_MEMCACHED_PORT}))" == "${SVN_TEST_FSFS_MEMCACHED_PORT}" ]]) &>/dev/null; then
			die "Value of SVN_TEST_FSFS_MEMCACHED_PORT must be an integer"
		fi
		if [[ -n "${SVN_TEST_FSFS_SHARDING}" ]] && ! ([[ "$((${SVN_TEST_FSFS_SHARDING}))" == "${SVN_TEST_FSFS_SHARDING}" ]]) &>/dev/null; then
			die "Value of SVN_TEST_FSFS_SHARDING must be an integer"
		fi
		if [[ -n "${SVN_TEST_FSFS_PACKING}" && -z "${SVN_TEST_FSFS_SHARDING}" ]]; then
			die "SVN_TEST_FSFS_PACKING requires SVN_TEST_FSFS_SHARDING"
		fi

		if [[ -n "${SVN_TEST_BINDINGS}" ]]; then
			if { use java || use perl || use python || use ruby; } && ! has_version "=${CATEGORY}/${PF}"; then
				die "${CATEGORY}/${PF} must be installed"
			fi
			if use java && ! has_version dev-java/junit:4; then
				die "dev-java/junit:4 must be installed"
			fi
		fi
	fi

	if use debug; then
		append-cppflags -DSVN_DEBUG -DAP_DEBUG
	fi
}

src_prepare() {
	epatch "${FILESDIR}/${PN}-1.6.0-disable_linking_against_unneeded_libraries.patch"
	epatch "${FILESDIR}/${PN}-1.6.2-local_library_preloading.patch"
	epatch "${FILESDIR}/${PN}-1.6.3-kwallet_window.patch"
	chmod +x build/transform_libtool_scripts.sh || die "chmod failed"

	if ! use test; then
		sed -i \
			-e "s/\(BUILD_RULES=.*\) bdb-test\(.*\)/\1\2/g" \
			-e "s/\(BUILD_RULES=.*\) test\(.*\)/\1\2/g" configure.ac
	fi

	eautoconf
	elibtoolize
}

src_configure() {
	local myconf

	if use python || use perl || use ruby; then
		myconf+=" --with-swig"
	else
		myconf+=" --without-swig"
	fi

	if use java; then
		if use test && [[ -n "${SVN_TEST_BINDINGS}" ]]; then
			myconf+=" --with-junit=/usr/share/junit-4/lib/junit.jar"
		else
			myconf+=" --without-junit"
		fi
	fi

	econf --libdir="/usr/$(get_libdir)" \
		$(use_with apache2 apxs "${APXS}") \
		$(use_with berkdb berkeley-db "db.h:/usr/include/db${SVN_BDB_VERSION}::db-${SVN_BDB_VERSION}") \
		$(use_with ctypes-python ctypesgen /usr) \
		$(use_enable dso runtime-module-search) \
		$(use_with gnome-keyring) \
		$(use_enable java javahl) \
		$(use_with java jdk "${JAVA_HOME}") \
		$(use_with kde kwallet) \
		$(use_enable nls) \
		$(use_with sasl) \
		$(use_with webdav-neon neon) \
		$(use_with webdav-serf serf /usr) \
		${myconf} \
		--with-apr=/usr/bin/apr-1-config \
		--with-apr-util=/usr/bin/apu-1-config \
		--disable-experimental-libtool \
		--without-jikes \
		--enable-local-library-preloading \
		--disable-mod-activation \
		--disable-neon-version-check \
		--with-sqlite=/usr
}

src_compile() {
	einfo
	einfo "Building of core of Subversion"
	einfo
	emake local-all || die "Building of core of Subversion failed"

	if use ctypes-python; then
		einfo
		einfo "Building of Subversion Ctypes Python bindings"
		einfo
		emake ctypes-python || die "Building of Subversion Ctypes Python bindings failed"
	fi

	if use python; then
		einfo
		einfo "Building of Subversion SWIG Python bindings"
		einfo
		emake swig_pydir="$(python_get_sitedir)/libsvn" swig_pydir_extra="$(python_get_sitedir)/svn" swig-py \
			|| die "Building of Subversion SWIG Python bindings failed"
	fi

	if use perl; then
		einfo
		einfo "Building of Subversion SWIG Perl bindings"
		einfo
		emake -j1 swig-pl || die "Building of Subversion SWIG Perl bindings failed"
	fi

	if use ruby; then
		einfo
		einfo "Building of Subversion SWIG Ruby bindings"
		einfo
		emake swig-rb || die "Building of Subversion SWIG Ruby bindings failed"
	fi

	if use java; then
		einfo
		einfo "Building of Subversion JavaHL library"
		einfo
		emake -j1 JAVAC_FLAGS="$(java-pkg_javac-args) -encoding iso8859-1" javahl \
			|| die "Building of Subversion JavaHL library failed"
	fi

	if use emacs; then
		einfo
		einfo "Compilation of Emacs modules"
		einfo
		elisp-compile contrib/client-side/emacs/{dsvn,psvn,vc-svn}.el doc/svn-doc.el doc/tools/svnbook.el || die "Compilation of Emacs modules failed"
	fi

	if use extras; then
		einfo
		einfo "Building of contrib and tools"
		einfo
		emake contrib || die "Building of contrib failed"
		emake tools || die "Building of tools failed"
	fi

	if use doc; then
		einfo
		einfo "Building of Subversion HTML documentation"
		einfo
		doxygen doc/doxygen.conf || die "Building of Subversion HTML documentation failed"

		if use java; then
			einfo
			einfo "Building of Subversion JavaHL library HTML documentation"
			einfo
			emake doc-javahl || die "Building of Subversion JavaHL library HTML documentation failed"
		fi
	fi
}

create_apache_tests_configuration() {
	get_loadmodule_directive() {
		if [[ "$("${APACHE_BIN}" -l)" != *"mod_$1.c"* ]]; then
			echo "LoadModule $1_module \"${APACHE_MODULESDIR}/mod_$1.so\""
		fi
	}
	get_loadmodule_directives() {
		if has_version "=www-servers/apache-2.4*"; then
			get_loadmodule_directive auth_basic
			get_loadmodule_directive authn_core
			get_loadmodule_directive authn_file
			get_loadmodule_directive authz_core
			get_loadmodule_directive authz_user
			get_loadmodule_directive dav
			get_loadmodule_directive log_config
			get_loadmodule_directive unixd
		else
			get_loadmodule_directive auth_basic
			get_loadmodule_directive authn_file
			get_loadmodule_directive dav
			get_loadmodule_directive log_config
		fi
	}

	mkdir -p "${T}/apache"
	cat << EOF > "${T}/apache/apache.conf"
$(get_loadmodule_directives)
LoadModule dav_svn_module "${S}/subversion/mod_dav_svn/.libs/mod_dav_svn.so"
LoadModule authz_svn_module "${S}/subversion/mod_authz_svn/.libs/mod_authz_svn.so"

User                $(id -un)
Group               $(id -gn)
Listen              localhost:${SVN_TEST_APACHE_PORT}
ServerName          localhost
ServerRoot          "${T}"
DocumentRoot        "${T}"
CoreDumpDirectory   "${T}"
PidFile             "${T}/apache.pid"
CustomLog           "${T}/apache/access_log" "%h %l %u %{%Y-%m-%dT%H:%M:%S}t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\""
CustomLog           "${T}/apache/svn_log" "%{%Y-%m-%dT%H:%M:%S}t %u %{SVN-REPOS-NAME}e %{SVN-ACTION}e" env=SVN-ACTION
ErrorLog            "${T}/apache/error_log"
LogLevel            Debug
MaxRequestsPerChild 0

<Directory />
	AllowOverride None
</Directory>

<Location /svn-test-work/repositories>
	DAV svn
	SVNParentPath "${S}/subversion/tests/cmdline/svn-test-work/repositories"
	AuthzSVNAccessFile "${S}/subversion/tests/cmdline/svn-test-work/authz"
	AuthType Basic
	AuthName "Subversion Repository"
	AuthUserFile "${T}/apache/users"
	Require valid-user
</Location>

<Location /svn-test-work/local_tmp/repos>
	DAV svn
	SVNPath "${S}/subversion/tests/cmdline/svn-test-work/local_tmp/repos"
	AuthzSVNAccessFile "${S}/subversion/tests/cmdline/svn-test-work/authz"
	AuthType Basic
	AuthName "Subversion Repository"
	AuthUserFile "${T}/apache/users"
	Require valid-user
</Location>
EOF

	cat << EOF > "${T}/apache/users"
jrandom:xCGl35kV9oWCY
jconstant:xCGl35kV9oWCY
EOF
}

set_tests_variables() {
	if [[ "$1" == "local" ]]; then
		base_url="file://${S}/subversion/tests/cmdline"
		http_library=""
	fi
	if [[ "$1" == "svn" ]]; then
		base_url="svn://127.0.0.1:${SVN_TEST_SVNSERVE_PORT}"
		http_library=""
	fi
	if [[ "$1" == "neon" || "$1" == "serf" ]]; then
		base_url="http://127.0.0.1:${SVN_TEST_APACHE_PORT}"
		http_library="$1"
	fi
}

src_test() {
	if ! use test; then
		die "Invalid configuration"
	fi

	local fs_type fs_types ra_type ra_types options failed_tests

	fs_types="fsfs"
	use berkdb && fs_types+=" bdb"

	ra_types="local svn"
	use webdav-neon && ra_types+=" neon"
	use webdav-serf && ra_types+=" serf"

	local pid_file
	for pid_file in svnserve.pid apache.pid memcached.pid; do
		rm -f "${T}/${pid_file}"
	done

	termination() {
		local die="$1" pid_file
		if [[ -n "${die}" ]]; then
			echo -e "\n\e[1;31mKilling of child processes...\e[0m\a" > /dev/tty
		fi
		for pid_file in svnserve.pid apache.pid memcached.pid; do
			if [[ -f "${T}/${pid_file}" ]]; then
				kill "$(<"${T}/${pid_file}")"
			fi
		done
		if [[ -n "${die}" ]]; then
			sleep 6
			die "Termination"
		fi
	}

	trap 'termination 1 &' SIGINT SIGTERM

	SVN_TEST_SVNSERVE_PORT="${SVN_TEST_SVNSERVE_PORT:-62209}"
	LC_ALL="C" subversion/svnserve/svnserve -dr "subversion/tests/cmdline" --listen-port "${SVN_TEST_SVNSERVE_PORT}" --log-file "${T}/svnserve.log" --pid-file "${T}/svnserve.pid"
	if use webdav-neon || use webdav-serf; then
		SVN_TEST_APACHE_PORT="${SVN_TEST_APACHE_PORT:-62208}"
		create_apache_tests_configuration
		"${APACHE_BIN}" -f "${T}/apache/apache.conf"
	fi
	if [[ -n "${SVN_TEST_FSFS_MEMCACHED}" ]]; then
		SVN_TEST_FSFS_MEMCACHED_PORT="${SVN_TEST_FSFS_MEMCACHED_PORT:-62210}"
		sed -e "/\[memcached-servers\]/akey = 127.0.0.1:${SVN_TEST_FSFS_MEMCACHED_PORT}" -i subversion/tests/tests.conf
		memcached -dp "${SVN_TEST_FSFS_MEMCACHED_PORT}" -P "${T}/memcached.pid"
	fi
	if [[ -n "${SVN_TEST_FSFS_SHARDING}" ]]; then
		options+=" FSFS_SHARDING=${SVN_TEST_FSFS_SHARDING}"
	fi
	if [[ -n "${SVN_TEST_FSFS_PACKING}" ]]; then
		options+=" FSFS_PACKING=1"
	fi
#	if [[ -n "${SVN_TEST_SASL}" ]]; then
#		options+=" ENABLE_SASL=1"
#	fi

	sleep 6

	for ra_type in ${ra_types}; do
		for fs_type in ${fs_types}; do
			[[ "${ra_type}" == "local" && "${fs_type}" == "bdb" ]] && continue
			einfo
			einfo "\e[1;34mTesting of ra_${ra_type} + $(echo ${fs_type} | tr '[:lower:]' '[:upper:]')\e[0m"
			einfo
			set_tests_variables ${ra_type}
			time emake check FS_TYPE="${fs_type}" BASE_URL="${base_url}" HTTP_LIBRARY="${http_library}" CLEANUP="1" ${options} || failed_tests="1"
			mv tests.log "${T}/tests-ra_${ra_type}-${fs_type}.log"
		done
	done
	unset base_url http_library
	termination
	trap - SIGINT SIGTERM

	if [[ -n "${SVN_TEST_BINDINGS}" ]]; then
		local swig_lingua swig_linguas
		local -A linguas
		if use ctypes-python; then
			einfo
			einfo "\e[1;34mTesting of Subversion Ctypes Python bindings\e[0m"
			einfo
			time emake check-ctypes-python || failed_tests="1"
		fi

		use perl && swig_linguas+=" pl"
		use python && swig_linguas+=" py"
		use ruby && swig_linguas+=" rb"

		linguas[pl]="Perl"
		linguas[py]="Python"
		linguas[rb]="Ruby"

		for swig_lingua in ${swig_linguas}; do
			einfo
			einfo "\e[1;34mTesting of Subversion SWIG ${linguas[${swig_lingua}]} bindings\e[0m"
			einfo
			time emake check-swig-${swig_lingua} || failed_tests="1"
		done

		if use java; then
			einfo
			einfo "\e[1;34mTesting of Subversion JavaHL library\e[0m"
			einfo
			time emake check-javahl || failed_tests="1"
		fi
	fi

	if [[ -n "${failed_tests}" ]]; then
		ewarn
		ewarn "\e[1;31mSome tests failed\e[0m"
		ewarn
		ebeep 12
	fi
}

src_install() {
	einfo
	einfo "Installation of core of Subversion"
	einfo
	emake -j1 DESTDIR="${D}" local-install || die "Installation of core of Subversion failed"

	if use ctypes-python; then
		einfo
		einfo "Installation of Subversion Ctypes Python bindings"
		einfo
		emake DESTDIR="${D}" install-ctypes-python || die "Installation of Subversion Ctypes Python bindings failed"
	fi

	if use python; then
		einfo
		einfo "Installation of Subversion SWIG Python bindings"
		einfo
		emake -j1 DESTDIR="${D}" swig_pydir="$(python_get_sitedir)/libsvn" swig_pydir_extra="$(python_get_sitedir)/svn" install-swig-py \
			|| die "Installation of Subversion SWIG Python bindings failed"
	fi

	if use perl; then
		einfo
		einfo "Installation of Subversion SWIG Perl bindings"
		einfo
		emake -j1 DESTDIR="${D}" INSTALLDIRS="vendor" install-swig-pl || die "Installation of Subversion SWIG Perl bindings failed"
		fixlocalpod
		find "${D}" "(" -name .packlist -o -name "*.bs" ")" -print0 | xargs -0 rm -fr
	fi

	if use ruby; then
		einfo
		einfo "Installation of Subversion SWIG Ruby bindings"
		einfo
		emake -j1 DESTDIR="${D}" install-swig-rb || die "Installation of Subversion SWIG Ruby bindings failed"
	fi

	if use java; then
		einfo
		einfo "Installation of Subversion JavaHL library"
		einfo
		emake -j1 DESTDIR="${D}" install-javahl || die "Installation of Subversion JavaHL library failed"
		java-pkg_regso "${D}"usr/$(get_libdir)/libsvnjavahl*.so
		java-pkg_dojar "${D}"usr/$(get_libdir)/svn-javahl/svn-javahl.jar
		rm -fr "${D}"usr/$(get_libdir)/svn-javahl/*.jar
	fi

	# Install Apache module configuration.
	if use apache2; then
		dodir "${APACHE_MODULES_CONFDIR}"
		cat << EOF > "${D}${APACHE_MODULES_CONFDIR}"/47_mod_dav_svn.conf
<IfDefine SVN>
LoadModule dav_svn_module modules/mod_dav_svn.so
<IfDefine SVN_AUTHZ>
LoadModule authz_svn_module modules/mod_authz_svn.so
</IfDefine>

# Example configuration:
#<Location /svn/repos>
#	DAV svn
#	SVNPath ${SVN_REPOS_LOC}/repos
#	AuthType Basic
#	AuthName "Subversion repository"
#	AuthUserFile ${SVN_REPOS_LOC}/conf/svnusers
#	Require valid-user
#</Location>
</IfDefine>
EOF
	fi

	# Install Bash Completion, bug 43179.
	dobashcompletion tools/client-side/bash_completion subversion
	rm -f tools/client-side/bash_completion

	# Install hot backup script, bug 54304.
	newbin tools/backup/hot-backup.py svn-hot-backup
	rm -fr tools/backup

	# Install svn_load_dirs.pl.
	if use perl; then
		dobin contrib/client-side/svn_load_dirs/svn_load_dirs.pl
	fi
	rm -f contrib/client-side/svn_load_dirs/svn_load_dirs.pl

	# Install svnserve init-script and xinet.d snippet, bug 43245.
	newinitd "${FILESDIR}"/svnserve.initd svnserve
	if use apache2; then
		newconfd "${FILESDIR}"/svnserve.confd svnserve
	else
		newconfd "${FILESDIR}"/svnserve.confd2 svnserve
	fi
	insinto /etc/xinetd.d
	newins "${FILESDIR}"/svnserve.xinetd svnserve

	# Install documentation.
	dodoc CHANGES COMMITTERS README
	dodoc tools/xslt/svnindex.{css,xsl}
	rm -fr tools/xslt

	# Install Vim syntax files.
	if use vim-syntax; then
		insinto /usr/share/vim/vimfiles/syntax
		doins contrib/client-side/vim/svn.vim
	fi
	rm -f contrib/client-side/vim/svn.vim

	# Install Emacs Lisps.
	if use emacs; then
		elisp-install ${PN} contrib/client-side/emacs/{dsvn,psvn}.{el,elc} doc/svn-doc.{el,elc} doc/tools/svnbook.{el,elc} || die "Installation of Emacs modules failed"
		elisp-install ${PN}/compat contrib/client-side/emacs/vc-svn.{el,elc} || die "Installation of Emacs modules failed"
		touch "${D}${SITELISP}/${PN}/compat/.nosearch"
		elisp-site-file-install "${FILESDIR}/70svn-gentoo.el" || die "Installation of Emacs site-init file failed"
	fi
	rm -fr contrib/client-side/emacs

	# Install extra files.
	if use extras; then
		einfo
		einfo "Installation of contrib and tools"
		einfo

		cat << EOF > 80subversion-extras
PATH="/usr/$(get_libdir)/subversion/bin"
ROOTPATH="/usr/$(get_libdir)/subversion/bin"
EOF
		doenvd 80subversion-extras

		emake DESTDIR="${D}" contribdir="/usr/$(get_libdir)/subversion/bin" install-contrib || die "Installation of contrib failed"
		emake DESTDIR="${D}" toolsdir="/usr/$(get_libdir)/subversion/bin" install-tools || die "Installation of tools failed"

		find contrib tools "(" -name "*.bat" -o -name "*.in" -o -name ".libs" ")" -print0 | xargs -0 rm -fr
		rm -fr contrib/client-side/svn-push
		rm -fr contrib/server-side/svnstsw
		rm -fr tools/client-side/svnmucc
		rm -fr tools/server-side/{svn-populate-node-origins-index,svnauthz-validate}*
		rm -fr tools/{buildbot,dev,diff,po}

		insinto /usr/share/${PN}
		doins -r contrib tools
	fi

	if use doc; then
		einfo
		einfo "Installation of Subversion HTML documentation"
		einfo
		dohtml -r doc/doxygen/html/* || die "Installation of Subversion HTML documentation failed"

		insinto /usr/share/doc/${PF}
		doins -r notes
		ecompressdir /usr/share/doc/${PF}/notes

#		if use ruby; then
#			emake DESTDIR="${D}" install-swig-rb-doc
#		fi

		if use java; then
			java-pkg_dojavadoc doc/javadoc
		fi
	fi
}

pkg_preinst() {
	# Compare versions of Berkeley DB, bug 122877.
	if use berkdb && [[ -f "${ROOT}usr/bin/svn" ]]; then
		OLD_BDB_VERSION="$(scanelf -nq "${ROOT}usr/$(get_libdir)/libsvn_subr-1.so.0" | grep -Eo "libdb-[[:digit:]]+\.[[:digit:]]+" | sed -e "s/libdb-\(.*\)/\1/")"
		NEW_BDB_VERSION="$(scanelf -nq "${D}usr/$(get_libdir)/libsvn_subr-1.so.0" | grep -Eo "libdb-[[:digit:]]+\.[[:digit:]]+" | sed -e "s/libdb-\(.*\)/\1/")"
		if [[ "${OLD_BDB_VERSION}" != "${NEW_BDB_VERSION}" ]]; then
			CHANGED_BDB_VERSION="1"
		fi
	fi
}

pkg_postinst() {
	use emacs && elisp-site-regen
	use perl && perl-module_pkg_postinst

	if use ctypes-python; then
		python_mod_optimize "$(python_get_sitedir)/csvn"
	fi

	elog "Subversion Server Notes"
	elog "-----------------------"
	elog
	elog "If you intend to run a server, a repository needs to be created using"
	elog "svnadmin (see man svnadmin) or the following command to create it in"
	elog "${SVN_REPOS_LOC}:"
	elog
	elog "    emerge --config =${CATEGORY}/${PF}"
	elog
	elog "Subversion has multiple server types, take your pick:"
	elog
	elog " - svnserve daemon: "
	elog "   1. Edit /etc/conf.d/svnserve"
	elog "   2. Fix the repository permissions (see \"Fixing the repository permissions\")"
	elog "   3. Start daemon: /etc/init.d/svnserve start"
	elog "   4. Make persistent: rc-update add svnserve default"
	elog
	elog " - svnserve via xinetd:"
	elog "   1. Edit /etc/xinetd.d/svnserve (remove disable line)"
	elog "   2. Fix the repository permissions (see \"Fixing the repository permissions\")"
	elog "   3. Restart xinetd.d: /etc/init.d/xinetd restart"
	elog
	elog " - svn over ssh:"
	elog "   1. Fix the repository permissions (see \"Fixing the repository permissions\")"
	elog "      Additionally run:"
	elog "        groupadd svnusers"
	elog "        chown -R root:svnusers ${SVN_REPOS_LOC}/repos"
	elog "   2. Create an svnserve wrapper in /usr/local/bin to set the umask you"
	elog "      want, for example:"
	elog "         #!/bin/bash"
	elog "         . /etc/conf.d/svnserve"
	elog "         umask 007"
	elog "         exec /usr/bin/svnserve \${SVNSERVE_OPTS} \"\$@\""
	elog

	if use apache2; then
		elog " - http-based server:"
		elog "   1. Edit /etc/conf.d/apache2 to include both \"-D DAV\" and \"-D SVN\""
		elog "   2. Create an htpasswd file:"
		elog "      htpasswd2 -m -c ${SVN_REPOS_LOC}/conf/svnusers USERNAME"
		elog "   3. Fix the repository permissions (see \"Fixing the repository permissions\")"
		elog "   4. Restart Apache: /etc/init.d/apache2 restart"
		elog
	fi

	elog " Fixing the repository permissions:"
	elog "      chmod -Rf go-rwx ${SVN_REPOS_LOC}/conf"
	elog "      chmod -Rf g-w,o-rwx ${SVN_REPOS_LOC}/repos"
	elog "      chmod -Rf g+rw ${SVN_REPOS_LOC}/repos/db"
	elog "      chmod -Rf g+rw ${SVN_REPOS_LOC}/repos/locks"
	elog

	elog "If you intend to use svn-hot-backup, you can specify the number of"
	elog "backups to keep per repository by specifying an environment variable."
	elog "If you want to keep e.g. 2 backups, do the following:"
	elog "echo '# hot-backup: Keep that many repository backups around' > /etc/env.d/80subversion"
	elog "echo 'SVN_HOTBACKUP_BACKUPS_NUMBER=2' >> /etc/env.d/80subversion"
	elog

	elog "Subversion contains support for the use of Memcached"
	elog "to cache data of FSFS repositories."
	elog "You should install \"net-misc/memcached\", start memcached"
	elog "and configure your FSFS repositories, if you want to use this feature."
	elog "See the documentation for details."
	elog
	epause 6

	if [[ -n "${CHANGED_BDB_VERSION}" ]]; then
		ewarn "You upgraded from an older version of Berkeley DB and may experience"
		ewarn "problems with your repository. Run the following commands as root to fix it:"
		ewarn "    db4_recover -h ${SVN_REPOS_LOC}/repos"
		ewarn "    chown -Rf apache:apache ${SVN_REPOS_LOC}/repos"
	fi
}

pkg_postrm() {
	use emacs && elisp-site-regen
	use perl && perl-module_pkg_postrm

	if use ctypes-python; then
		python_mod_cleanup "$(python_get_sitedir)/csvn"
	fi
}

pkg_config() {
	einfo ">>> Initializing the database in ${ROOT}${SVN_REPOS_LOC} ..."
	if [[ -e "${ROOT}${SVN_REPOS_LOC}/repos" ]]; then
		echo "A Subversion repository already exists and I will not overwrite it."
		echo "Delete \"${ROOT}${SVN_REPOS_LOC}/repos\" first if you're sure you want to have a clean version."
	else
		mkdir -p "${ROOT}${SVN_REPOS_LOC}/conf"

		einfo ">>> Populating repository directory ..."
		# Create initial repository.
		"${ROOT}usr/bin/svnadmin" create "${ROOT}${SVN_REPOS_LOC}/repos"

		einfo ">>> Setting repository permissions ..."
		SVNSERVE_USER="$(. "${ROOT}etc/conf.d/svnserve"; echo "${SVNSERVE_USER}")"
		SVNSERVE_GROUP="$(. "${ROOT}etc/conf.d/svnserve"; echo "${SVNSERVE_GROUP}")"
		if use apache2; then
			[[ -z "${SVNSERVE_USER}" ]] && SVNSERVE_USER="apache"
			[[ -z "${SVNSERVE_GROUP}" ]] && SVNSERVE_GROUP="apache"
		else
			[[ -z "${SVNSERVE_USER}" ]] && SVNSERVE_USER="svn"
			[[ -z "${SVNSERVE_GROUP}" ]] && SVNSERVE_GROUP="svnusers"
			enewgroup "${SVNSERVE_GROUP}"
			enewuser "${SVNSERVE_USER}" -1 -1 "${SVN_REPOS_LOC}" "${SVNSERVE_GROUP}"
		fi
		chown -Rf "${SVNSERVE_USER}:${SVNSERVE_GROUP}" "${ROOT}${SVN_REPOS_LOC}/repos"
		chmod -Rf go-rwx "${ROOT}${SVN_REPOS_LOC}/conf"
		chmod -Rf o-rwx "${ROOT}${SVN_REPOS_LOC}/repos"
	fi
}
