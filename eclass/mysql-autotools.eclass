# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/mysql-autotools.eclass,v 1.14 2013/01/28 02:13:05 robbat2 Exp $

# @ECLASS: mysql-autotools.eclass
# @MAINTAINER:
# MySQL Team <mysql-bugs@gentoo.org>
# Robin H. Johnson <robbat2@gentoo.org>
# Jorge Manuel B. S. Vicetto <jmbsvicetto@gentoo.org>
# Luca Longinotti <chtekk@gentoo.org>
# @AUTHOR:
# Francesco Riosa <vivo@gentoo.org> (retired)
# @BLURB: This eclass provides support for autotools based mysql releases
# @DESCRIPTION:
# The mysql-autotools.eclass provides the support to build the mysql
# ebuilds using the autotools build system. This eclass provides
# the src_unpack, src_prepare, src_configure, src_compile, scr_install,
# pkg_preinst, pkg_postinst, pkg_config and pkg_postrm phase hooks.

inherit autotools flag-o-matic multilib prefix

#
# HELPER FUNCTIONS:
#

# @FUNCTION: mysql-autotools_disable_test
# @DESCRIPTION:
# Helper function to disable specific tests.
mysql-autotools_disable_test() {

	local rawtestname testname testsuite reason mysql_disable_file
	rawtestname="${1}" ; shift
	reason="${@}"
	ewarn "test '${rawtestname}' disabled: '${reason}'"

	testsuite="${rawtestname/.*}"
	testname="${rawtestname/*.}"
	mysql_disable_file="${S}/mysql-test/t/disabled.def"
	#einfo "rawtestname=${rawtestname} testname=${testname} testsuite=${testsuite}"
	echo "${testname} : ${reason}" >> "${mysql_disable_file}"

	# ${S}/mysql-tests/t/disabled.def
	#
	# ${S}/mysql-tests/suite/federated/disabled.def
	#
	# ${S}/mysql-tests/suite/jp/t/disabled.def
	# ${S}/mysql-tests/suite/ndb/t/disabled.def
	# ${S}/mysql-tests/suite/rpl/t/disabled.def
	# ${S}/mysql-tests/suite/parts/t/disabled.def
	# ${S}/mysql-tests/suite/rpl_ndb/t/disabled.def
	# ${S}/mysql-tests/suite/ndb_team/t/disabled.def
	# ${S}/mysql-tests/suite/binlog/t/disabled.def
	# ${S}/mysql-tests/suite/innodb/t/disabled.def
	if [ -n "${testsuite}" ]; then
		for mysql_disable_file in \
			${S}/mysql-test/suite/${testsuite}/disabled.def  \
			${S}/mysql-test/suite/${testsuite}/t/disabled.def  \
			FAILED ; do
			[ -f "${mysql_disable_file}" ] && break
		done
		if [ "${mysql_disabled_file}" != "FAILED" ]; then
			echo "${testname} : ${reason}" >> "${mysql_disable_file}"
		else
			ewarn "Could not find testsuite disabled.def location for ${rawtestname}"
		fi
	fi
}

# @FUNCTION: mysql-autotools_configure_minimal
# @DESCRIPTION:
# Helper function to configure a minimal build
mysql-autotools_configure_minimal() {

	# These are things we exclude from a minimal build, please
	# note that the server actually does get built and installed,
	# but we then delete it before packaging.
	local minimal_exclude_list="server embedded-server extra-tools innodb bench berkeley-db row-based-replication readline"

	for i in ${minimal_exclude_list} ; do
		myconf="${myconf} --without-${i}"
	done
	myconf="${myconf} --with-extra-charsets=none"
	myconf="${myconf} --enable-local-infile"

	if use static ; then
		myconf="${myconf} --with-client-ldflags=-all-static"
		myconf="${myconf} --disable-shared --with-pic"
	else
		myconf="${myconf} --enable-shared --enable-static"
	fi

	if ! use latin1 ; then
		myconf="${myconf} --with-charset=utf8"
		myconf="${myconf} --with-collation=utf8_general_ci"
	else
		myconf="${myconf} --with-charset=latin1"
		myconf="${myconf} --with-collation=latin1_swedish_ci"
	fi

	# MariaDB requires this flag in order to link to GPLv3 readline v6 or greater
	# A note is added to the configure output
	if [[ "${PN}" == "mariadb" ]]  && mysql_version_is_at_least "5.1.61" ; then
		myconf="${myconf} --disable-distribution"
	fi
}

# @FUNCTION: mysql-autotools_configure_common
# @DESCRIPTION:
# Helper function to configure the common builds
mysql-autotools_configure_common() {

	myconf="${myconf} $(use_with big-tables)"
	myconf="${myconf} --enable-local-infile"
	myconf="${myconf} --with-extra-charsets=all"
	myconf="${myconf} --with-mysqld-user=mysql"
	myconf="${myconf} --with-server"
	myconf="${myconf} --with-unix-socket-path=${EPREFIX}/var/run/mysqld/mysqld.sock"
	myconf="${myconf} --without-libwrap"

	if use static ; then
		myconf="${myconf} --with-mysqld-ldflags=-all-static"
		myconf="${myconf} --with-client-ldflags=-all-static"
		myconf="${myconf} --disable-shared --with-pic"
	else
		myconf="${myconf} --enable-shared --enable-static"
	fi

	if use debug ; then
		myconf="${myconf} --with-debug=full"
	else
		myconf="${myconf} --without-debug"
		if ( use cluster ); then
			myconf="${myconf} --without-ndb-debug"
		fi
	fi

	if [ -n "${MYSQL_DEFAULT_CHARSET}" -a -n "${MYSQL_DEFAULT_COLLATION}" ]; then
		ewarn "You are using a custom charset of ${MYSQL_DEFAULT_CHARSET}"
		ewarn "and a collation of ${MYSQL_DEFAULT_COLLATION}."
		ewarn "You MUST file bugs without these variables set."
		myconf="${myconf} --with-charset=${MYSQL_DEFAULT_CHARSET}"
		myconf="${myconf} --with-collation=${MYSQL_DEFAULT_COLLATION}"
	elif ! use latin1 ; then
		myconf="${myconf} --with-charset=utf8"
		myconf="${myconf} --with-collation=utf8_general_ci"
	else
		myconf="${myconf} --with-charset=latin1"
		myconf="${myconf} --with-collation=latin1_swedish_ci"
	fi

	if use embedded ; then
		myconf="${myconf} --with-embedded-privilege-control"
		myconf="${myconf} --with-embedded-server"
	else
		myconf="${myconf} --without-embedded-privilege-control"
		myconf="${myconf} --without-embedded-server"
	fi

}

# @FUNCTION: mysql-autotools_configure_51
# @DESCRIPTION:
# Helper function to configure 5.1 and later builds
mysql-autotools_configure_51() {

	# TODO: !!!! readd --without-readline
	# the failure depend upon config/ac-macros/readline.m4 checking into
	# readline.h instead of history.h
	myconf="${myconf} $(use_with ssl ssl "${EPREFIX}"/usr)"
	myconf="${myconf} --enable-assembler"
	myconf="${myconf} --with-geometry"
	myconf="${myconf} --with-readline"
	myconf="${myconf} --with-zlib-dir=${EPREFIX}/usr/"
	myconf="${myconf} --without-pstack"
	myconf="${myconf} --with-plugindir=${EPREFIX}/usr/$(get_libdir)/mysql/plugin"

	# This is an explict die here, because if we just forcibly disable it, then the
	# user's data is not accessible.
	use max-idx-128 && die "Bug #336027: upstream has a corruption issue with max-idx-128 presently"
	#use max-idx-128 && myconf="${myconf} --with-max-indexes=128"
	myconf="${myconf} $(use_enable community community-features)"
	if use community; then
		myconf="${myconf} $(use_enable profiling)"
	else
		myconf="${myconf} --disable-profiling"
	fi

	# Scan for all available plugins
	local plugins_avail="$(
	LANG=C \
	find "${S}" \
		\( \
		-name 'plug.in' \
		-o -iname 'configure.in' \
		-o -iname 'configure.ac' \
		\) \
		-print0 \
	| xargs -0 sed -r -n \
		-e '/^MYSQL_STORAGE_ENGINE/{
			s~MYSQL_STORAGE_ENGINE\([[:space:]]*\[?([-_a-z0-9]+)\]?.*,~\1 ~g ;
			s~^([^ ]+).*~\1~gp;
		}' \
	| tr -s '\n' ' '
	)"

	# 5.1 introduces a new way to manage storage engines (plugins)
	# like configuration=none
	# This base set are required, and will always be statically built.
	local plugins_sta="csv myisam myisammrg heap"
	local plugins_dyn=""
	local plugins_dis="example ibmdb2i"

	# These aren't actually required by the base set, but are really useful:
	plugins_sta="${plugins_sta} archive blackhole"

	# Now the extras
	if use extraengine ; then
		# like configuration=max-no-ndb, archive and example removed in 5.1.11
		# not added yet: ibmdb2i
		# Not supporting as examples: example,daemon_example,ftexample
		plugins_sta="${plugins_sta} partition"

		if [[ "${PN}" != "mariadb" ]] ; then
			elog "Before using the Federated storage engine, please be sure to read"
			elog "http://dev.mysql.com/doc/refman/5.1/en/federated-limitations.html"
			plugins_dyn="${plugins_dyn} federated"
		else
			elog "MariaDB includes the FederatedX engine. Be sure to read"
			elog "http://askmonty.org/wiki/index.php/Manual:FederatedX_storage_engine"
			plugins_dyn="${plugins_dyn} federatedx"
		fi
	else
		plugins_dis="${plugins_dis} partition federated"
	fi

	# Upstream specifically requests that InnoDB always be built:
	# - innobase, innodb_plugin
	# Build falcon if available for 6.x series.
	for i in innobase falcon ; do
		[ -e "${S}"/storage/${i} ] && plugins_sta="${plugins_sta} ${i}"
	done
	for i in innodb_plugin ; do
		[ -e "${S}"/storage/${i} ] && plugins_dyn="${plugins_dyn} ${i}"
	done

	# like configuration=max-no-ndb
	if ( use cluster ) ; then
		plugins_sta="${plugins_sta} ndbcluster partition"
		plugins_dis="${plugins_dis//partition}"
		myconf="${myconf} --with-ndb-binlog"
	else
		plugins_dis="${plugins_dis} ndbcluster"
	fi

	if [[ "${PN}" == "mariadb" ]] ; then
		# In MariaDB, InnoDB is packaged in the xtradb directory, so it's not
		# caught above.
		# This is not optional, without it several upstream testcases fail.
		# Also strongly recommended by upstream.
		if [[ "${PV}" < "5.2.0" ]] ; then
			myconf="${myconf} --with-maria-tmp-tables"
			plugins_sta="${plugins_sta} maria"
		else
			myconf="${myconf} --with-aria-tmp-tables"
			plugins_sta="${plugins_sta} aria"
		fi

		[ -e "${S}"/storage/innobase ] || [ -e "${S}"/storage/xtradb ] ||
			die "The ${P} package doesn't provide innobase nor xtradb"

		for i in innobase xtradb ; do
			[ -e "${S}"/storage/${i} ] && plugins_sta="${plugins_sta} ${i}"
		done

		myconf="${myconf} $(use_with libevent)"

		if mysql_version_is_at_least "5.2" ; then
			for i in oqgraph ; do
				use ${i} \
				&& plugins_dyn="${plugins_dyn} ${i}" \
				|| plugins_dis="${plugins_dis} ${i}"
			done
		fi

		if mysql_version_is_at_least "5.2.5" ; then
			for i in sphinx ; do
				use ${i} \
				&& plugins_dyn="${plugins_dyn} ${i}" \
				|| plugins_dis="${plugins_dis} ${i}"
			done
		fi
	fi

	if pbxt_available && [[ "${PBXT_NEWSTYLE}" == "1" ]]; then
		use pbxt \
		&& plugins_sta="${plugins_sta} pbxt" \
		|| plugins_dis="${plugins_dis} pbxt"
	fi

	use static && \
	plugins_sta="${plugins_sta} ${plugins_dyn}" && \
	plugins_dyn=""

	# Google MySQL, bundle what upstream supports
	if [[ "${PN}" == "google-mysql" ]]; then
		for x in innobase innodb_plugin innodb ; do
			plugins_sta="${plugins_sta//$x}"
			plugins_dyn="${plugins_dyn//$x}"
		done
		plugins_sta="${plugins_sta} innodb_plugin googlestats"
		myconf="${myconf} --with-perftools-dir=/usr --enable-perftools-tcmalloc"
		# use system lzo for google-mysql
		myconf="${myconf} --with-lzo2-dir=/usr"
	fi

	einfo "Available plugins: ${plugins_avail}"
	einfo "Dynamic plugins: ${plugins_dyn}"
	einfo "Static plugins: ${plugins_sta}"
	einfo "Disabled plugins: ${plugins_dis}"

	# These are the static plugins
	myconf="${myconf} --with-plugins=${plugins_sta// /,}"
	# And the disabled ones
	for i in ${plugins_dis} ; do
		myconf="${myconf} --without-plugin-${i}"
	done
}

pbxt_src_configure() {

	mysql_init_vars

	pushd "${WORKDIR}/pbxt-${PBXT_VERSION}" &>/dev/null

	einfo "Reconfiguring dir '${PWD}'"
	eautoreconf

	local myconf=""
	myconf="${myconf} --with-mysql=${S} --libdir=${EPREFIX}/usr/$(get_libdir)"
	use debug && myconf="${myconf} --with-debug=full"
	econf ${myconf} || die "Problem configuring PBXT storage engine"
}

pbxt_src_compile() {

	# TODO: is it safe/needed to use emake here ?
	make || die "Problem making PBXT storage engine (${myconf})"

	popd
	# TODO: modify test suite for PBXT
}

pbxt_src_install() {

	pushd "${WORKDIR}/pbxt-${PBXT_VERSION}" &>/dev/null
		emake install DESTDIR="${D}" || die "Failed to install PBXT"
	popd
}

#
# EBUILD FUNCTIONS
#

# @FUNCTION: mysql-autotools_src_prepare
# @DESCRIPTION:
# Apply patches to the source code and remove unneeded bundled libs.
mysql-autotools_src_prepare() {

	cd "${S}"

	# Apply the patches for this MySQL version
	EPATCH_SUFFIX="patch"
	mkdir -p "${EPATCH_SOURCE}" || die "Unable to create epatch directory"
	# Clean out old items
	rm -f "${EPATCH_SOURCE}"/*
	# Now link in right patches
	mysql_mv_patches
	# And apply
	epatch

	# last -fPIC fixup, per bug #305873
	i="${S}"/storage/innodb_plugin/plug.in
	[ -f "${i}" ] && sed -i -e '/CFLAGS/s,-prefer-non-pic,,g' "${i}"

	# Additional checks, remove bundled zlib
	rm -f "${S}/zlib/"*.[ch]
	sed -i -e "s/zlib\/Makefile dnl/dnl zlib\/Makefile/" "${S}/configure.in"
	rm -f "scripts/mysqlbug"

	# Make charsets install in the right place
	find . -name 'Makefile.am' \
		-exec sed --in-place -e 's!$(pkgdatadir)!'${MY_SHAREDSTATEDIR}'!g' {} \;

	# Remove what needs to be recreated, so we're sure it's actually done
	einfo "Cleaning up old buildscript files"
	find . -name Makefile \
		-o -name Makefile.in \
		-o -name configure \
		-exec rm -f {} \;
	rm -f "ltmain.sh"
	rm -f "scripts/mysqlbug"

	local rebuilddirlist d

	if xtradb_patch_available && use xtradb ; then
		einfo "Adding storage engine: Percona XtraDB (replacing InnoDB)"
		pushd "${S}"/storage >/dev/null
		i="innobase"
		o="${WORKDIR}/storage-${i}.mysql-upstream"
		# Have we been here already?
		[ -d "${o}" ] && rm -f "${i}"
		# Or maybe we haven't
		[ -d "${i}" -a ! -d "${o}" ] && mv "${i}" "${o}"
		cp -ral "${WORKDIR}/${XTRADB_P}" "${i}"
		popd >/dev/null
	fi

	if pbxt_patch_available && [[ "${PBXT_NEWSTYLE}" == "1" ]] && use pbxt ; then
		einfo "Adding storage engine: PBXT"
		pushd "${S}"/storage >/dev/null
		i='pbxt'
		[ -d "${i}" ] && rm -rf "${i}"
		cp -ral "${WORKDIR}/${PBXT_P}" "${i}"
		f="${WORKDIR}/mysql-extras/pbxt/fix-low-priority.patch"
		[[ -f $f ]] && epatch "$f" 
		popd >/dev/null
	fi

	rebuilddirlist="."
	# This does not seem to be needed presently. robbat2 2010/02/23
	#einfo "Updating innobase cmake"
	## TODO: check this with a cmake expert
	#cmake \
	#	-DCMAKE_C_COMPILER=$(type -P $(tc-getCC)) \
	#	-DCMAKE_CXX_COMPILER=$(type -P $(tc-getCXX)) \
	#	"storage/innobase"

	for d in ${rebuilddirlist} ; do
		einfo "Reconfiguring dir '${d}'"
		pushd "${d}" &>/dev/null
		eautoreconf
		popd &>/dev/null
	done
}

# @FUNCTION: mysql-autotools_src_configure
# @DESCRIPTION:
# Configure mysql to build the code for Gentoo respecting the use flags.
mysql-autotools_src_configure() {

	# Make sure the vars are correctly initialized
	mysql_init_vars

	# $myconf is modified by the configure_* functions
	local myconf=""

	if use minimal ; then
		mysql-autotools_configure_minimal
	else
		mysql-autotools_configure_common
		mysql-autotools_configure_51
	fi

	# Bug #114895, bug #110149
	filter-flags "-O" "-O[01]"

	# glib-2.3.2_pre fix, bug #16496
	append-flags "-DHAVE_ERRNO_AS_DEFINE=1"

	# As discovered by bug #246652, doing a double-level of SSP causes NDB to
	# fail badly during cluster startup.
	if [[ $(gcc-major-version) -lt 4 ]]; then
		filter-flags "-fstack-protector-all"
	fi

	CXXFLAGS="${CXXFLAGS} -fno-exceptions -fno-strict-aliasing"
	CXXFLAGS="${CXXFLAGS} -felide-constructors -fno-rtti"
	# storage/googlestats, sql/ in google-mysql are using C++ templates
	# implicitly. Upstream might be interested in this, exclude
	# -fno-implicit-templates for google-mysql for now.
	mysql_version_is_at_least "5.0" \
	&& [[ "${PN}" != "google-mysql" ]] \
	&& CXXFLAGS="${CXXFLAGS} -fno-implicit-templates"
	export CXXFLAGS

	# bug #283926, with GCC4.4, this is required to get correct behavior.
	append-flags -fno-strict-aliasing

	# bug #335185, #335995, with >= GCC4.3.3 on x86 only, omit-frame-pointer
	# causes a mis-compile.
	# Upstream bugs:
	# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=38562
	# http://bugs.mysql.com/bug.php?id=45205
	use x86 && version_is_at_least "4.3.3" "$(gcc-fullversion)" && \
		append-flags -fno-omit-frame-pointer && \
		filter-flags -fomit-frame-pointer

	econf \
		--libexecdir="${EPREFIX}/usr/sbin" \
		--sysconfdir="${MY_SYSCONFDIR}" \
		--localstatedir="${MY_LOCALSTATEDIR}" \
		--sharedstatedir="${MY_SHAREDSTATEDIR}" \
		--libdir="${MY_LIBDIR}" \
		--includedir="${MY_INCLUDEDIR}" \
		--with-low-memory \
		--with-client-ldflags=-lstdc++ \
		--enable-thread-safe-client \
		--with-comment="Gentoo Linux ${PF}" \
		--without-docs \
		--with-LIBDIR="$(get_libdir)" \
		${myconf} || die "econf failed"

	# TODO: Move this before autoreconf !!!
	find . -type f -name Makefile -print0 \
	| xargs -0 -n100 sed -i \
	-e 's|^pkglibdir *= *$(libdir)/mysql|pkglibdir = $(libdir)|;s|^pkgincludedir *= *$(includedir)/mysql|pkgincludedir = $(includedir)|'

	if [[ $EAPI == 2 ]] && [[ "${PBXT_NEWSTYLE}" != "1" ]]; then
		pbxt_patch_available && use pbxt && pbxt_src_configure
	fi
}

# @FUNCTION: mysql-autotools_src_compile
# @DESCRIPTION:
# Compile the mysql code.
mysql-autotools_src_compile() {

	emake || die "emake failed"

	if [[ "${PBXT_NEWSTYLE}" != "1" ]]; then
		pbxt_patch_available && use pbxt && pbxt_src_compile
	fi
}

# @FUNCTION: mysql-autotools_src_install
# @DESCRIPTION:
# Install mysql.
mysql-autotools_src_install() {

	# Make sure the vars are correctly initialized
	mysql_init_vars

	emake install \
		DESTDIR="${D}" \
		benchdir_root="${MY_SHAREDSTATEDIR}" \
		testroot="${MY_SHAREDSTATEDIR}" \
		|| die "emake install failed"

	if [[ "${PBXT_NEWSTYLE}" != "1" ]]; then
		pbxt_patch_available && use pbxt && pbxt_src_install
	fi

	# Convenience links
	einfo "Making Convenience links for mysqlcheck multi-call binary"
	dosym "/usr/bin/mysqlcheck" "/usr/bin/mysqlanalyze"
	dosym "/usr/bin/mysqlcheck" "/usr/bin/mysqlrepair"
	dosym "/usr/bin/mysqlcheck" "/usr/bin/mysqloptimize"

	# Various junk (my-*.cnf moved elsewhere)
	einfo "Removing duplicate /usr/share/mysql files"
	rm -Rf "${ED}/usr/share/info"
	for removeme in  "mysql-log-rotate" mysql.server* \
		binary-configure* my-*.cnf mi_test_all*
	do
		rm -f "${D}"/${MY_SHAREDSTATEDIR}/${removeme}
	done

	# Clean up stuff for a minimal build
	if use minimal ; then
		einfo "Remove all extra content for minimal build"
		rm -Rf "${D}${MY_SHAREDSTATEDIR}"/{mysql-test,sql-bench}
		rm -f "${ED}"/usr/bin/{mysql{_install_db,manager*,_secure_installation,_fix_privilege_tables,hotcopy,_convert_table_format,d_multi,_fix_extensions,_zap,_explain_log,_tableinfo,d_safe,_install,_waitpid,binlog,test},myisam*,isam*,pack_isam}
		rm -f "${ED}/usr/sbin/mysqld"
		rm -f "${D}${MY_LIBDIR}"/lib{heap,merge,nisam,my{sys,strings,sqld,isammrg,isam},vio,dbug}.a
	fi

	# Unless they explicitly specific USE=test, then do not install the
	# testsuite. It DOES have a use to be installed, esp. when you want to do a
	# validation of your database configuration after tuning it.
	if use !test ; then
		rm -rf "${D}"/${MY_SHAREDSTATEDIR}/mysql-test
	fi

	# Configuration stuff
	case ${MYSQL_PV_MAJOR} in
		5.[1-9]|6*|7*) mysql_mycnf_version="5.1" ;;
	esac
	einfo "Building default my.cnf (${mysql_mycnf_version})"
	insinto "${MY_SYSCONFDIR#${EPREFIX}}"
	doins scripts/mysqlaccess.conf
	mycnf_src="my.cnf-${mysql_mycnf_version}"
	sed -e "s!@DATADIR@!${MY_DATADIR}!g" \
		-e "s!/tmp!${EPREFIX}/tmp!" \
		-e "s!/usr!${EPREFIX}/usr!" \
		-e "s!= /var!= ${EPREFIX}/var!" \
		"${FILESDIR}/${mycnf_src}" \
		> "${TMPDIR}/my.cnf.ok"
	if use latin1 ; then
		sed -i \
			-e "/character-set/s|utf8|latin1|g" \
			"${TMPDIR}/my.cnf.ok"
	fi
	eprefixify "${TMPDIR}/my.cnf.ok"
	newins "${TMPDIR}/my.cnf.ok" my.cnf

	# Minimal builds don't have the MySQL server
	if ! use minimal ; then
		einfo "Creating initial directories"
		# Empty directories ...
		diropts "-m0750"
		keepdir "${MY_DATADIR#${EPREFIX}}"
		if [[ "${PREVIOUS_DATADIR}" != "yes" ]] ; then
			chown -R mysql:mysql "${D}/${MY_DATADIR}"
		fi

		diropts "-m0755"
		for folder in "${MY_LOGDIR#${EPREFIX}}" "/var/run/mysqld" ; do
			dodir "${folder}"
			keepdir "${folder}"
			chown -R mysql:mysql "${ED}/${folder}"
		done
	fi

	# Docs
	einfo "Installing docs"
	for i in README ChangeLog EXCEPTIONS-CLIENT INSTALL-SOURCE ; do
		[[ -f "$i" ]] && dodoc "$i"
	done
	doinfo "${S}"/Docs/mysql.info

	# Minimal builds don't have the MySQL server
	if ! use minimal ; then
		einfo "Including support files and sample configurations"
		docinto "support-files"
		for script in \
			"${S}"/support-files/my-*.cnf \
			"${S}"/support-files/magic \
			"${S}"/support-files/ndb-config-2-node.ini
		do
			[[ -f "$script" ]] && dodoc "${script}"
		done

		docinto "scripts"
		for script in "${S}"/scripts/mysql* ; do
			[[ -f "$script" ]] && [[ "${script%.sh}" == "${script}" ]] && dodoc "${script}"
		done

	fi

	mysql_lib_symlinks "${ED}"
}
