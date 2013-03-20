# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/portage/portage-2.1.6.7_p1.ebuild,v 1.5 2013/01/05 01:19:27 zmedico Exp $

inherit eutils multilib python

DESCRIPTION="Portage is the package management and distribution system for Gentoo"
HOMEPAGE="http://www.gentoo.org/proj/en/portage/index.xml"
LICENSE="GPL-2"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc ~sparc-fbsd x86 ~x86-fbsd"
SLOT="0"
IUSE="build doc epydoc less selinux linguas_pl"

python_dep=">=dev-lang/python-2.4 <dev-lang/python-3"

DEPEND="${python_dep}
	>=sys-apps/sed-4.0.5 sys-devel/patch
	doc? ( app-text/xmlto ~app-text/docbook-xml-dtd-4.4 )
	epydoc? ( >=dev-python/epydoc-2.0 )"
# the debugedit blocker is for bug #289967
RDEPEND="${python_dep}
	!build? ( >=sys-apps/sed-4.0.5
		>=app-shells/bash-3.2_p17
		>=app-admin/eselect-1.2 )
	elibc_FreeBSD? ( sys-freebsd/freebsd-bin )
	elibc_glibc? ( >=sys-apps/sandbox-1.2.17 !mips? ( >=sys-apps/sandbox-1.2.18.1-r2 ) )
	elibc_uclibc? ( >=sys-apps/sandbox-1.2.17 !mips? ( >=sys-apps/sandbox-1.2.18.1-r2 ) )
	>=app-misc/pax-utils-0.1.17
	selinux? ( >=dev-python/python-selinux-2.16 )
	!>=dev-util/debugedit-4.4.6-r2"
PDEPEND="
	!build? (
		less? ( sys-apps/less )
		>=net-misc/rsync-2.6.4
		userland_GNU? ( >=sys-apps/coreutils-6.4 )
	)"
# coreutils-6.4 rdep is for date format in emerge-webrsync #164532
# NOTE: FEATURES=installsources requires debugedit and rsync

SRC_ARCHIVES="http://dev.gentoo.org/~zmedico/portage/archives"

prefix_src_archives() {
	local x y
	for x in ${@}; do
		for y in ${SRC_ARCHIVES}; do
			echo ${y}/${x}
		done
	done
}

PV_PL="2.1.2"
PATCHVER_PL=""
TARBALL_PV=2.1.6
SRC_URI="mirror://gentoo/${PN}-${TARBALL_PV}.tar.bz2
	$(prefix_src_archives ${PN}-${TARBALL_PV}.tar.bz2)
	linguas_pl? ( mirror://gentoo/${PN}-man-pl-${PV_PL}.tar.bz2
		$(prefix_src_archives ${PN}-man-pl-${PV_PL}.tar.bz2) )"

for PATCHVER in 2.1.6.7 ${PV} ; do
	SRC_URI="${SRC_URI} mirror://gentoo/${PN}-${PATCHVER}.patch.bz2
	$(prefix_src_archives ${PN}-${PATCHVER}.patch.bz2)"
done

S="${WORKDIR}"/${PN}-${TARBALL_PV}
S_PL="${WORKDIR}"/${PN}-${PV_PL}

pkg_setup() {
	# Bug #359731 - Die early if get_libdir fails.
	[[ -z $(get_libdir) ]] && \
		die "get_libdir returned an empty string"
}

src_unpack() {
	unpack ${A}
	cd "${S}"
	for PATCHVER in 2.1.6.7 ${PV} ; do
		epatch "${WORKDIR}/${PN}-${PATCHVER}.patch"
	done
	einfo "Setting portage.VERSION to ${PVR} ..."
	sed -i "s/^VERSION=.*/VERSION=\"${PVR}\"/" pym/portage/__init__.py || \
		die "Failed to patch portage.VERSION"
}

src_compile() {

	if use doc; then
		cd "${S}"/doc
		touch fragment/date
		# Workaround for bug #272063, remove in 2.1.6.14.
		sed 's:^XMLTO_FLAGS =:XMLTO_FLAGS = --skip-validation:' -i Makefile
		make xhtml xhtml-nochunks || die "failed to make docs"
	fi

	if use epydoc; then
		einfo "Generating api docs"
		mkdir "${WORKDIR}"/api
		local my_modules epydoc_opts=""
		# A name collision between the portage.dbapi class and the
		# module with the same name triggers an epydoc crash unless
		# portage.dbapi is excluded from introspection.
		ROOT=/ has_version '>=dev-python/epydoc-3_pre0' && \
			epydoc_opts='--exclude-introspect portage\.dbapi'
		my_modules="$(find "${S}/pym" -name "*.py" \
			| sed -e 's:/__init__.py$::' -e 's:\.py$::' -e "s:^${S}/pym/::" \
			 -e 's:/:.:g' | sort)" || die "error listing modules"
		# workaround for bug 282760
		> "$S/pym/pysqlite2.py"
		PYTHONPATH="${S}/pym:${PYTHONPATH}" epydoc -o "${WORKDIR}"/api \
			-qqqqq --no-frames --show-imports $epydoc_opts \
			--name "${PN}" --url "${HOMEPAGE}" \
			${my_modules} || die "epydoc failed"
		rm "$S/pym/pysqlite2.py"
	fi
}

src_test() {
	./pym/portage/tests/runTests || \
		die "test(s) failed"
}

src_install() {
	local libdir=$(get_libdir)
	local portage_base="/usr/${libdir}/portage"
	local portage_share_config=/usr/share/portage/config

	cd "${S}"/cnf
	insinto /etc
	doins etc-update.conf dispatch-conf.conf

	dodir "${portage_share_config}"
	insinto "${portage_share_config}"
	doins "${S}/cnf/"make.globals
	if [ -f "make.conf.${ARCH}".diff ]; then
		patch make.conf "make.conf.${ARCH}".diff || \
			die "Failed to patch make.conf.example"
		newins make.conf make.conf.example
	else
		eerror ""
		eerror "Portage does not have an arch-specific configuration for this arch."
		eerror "Please notify the arch maintainer about this issue. Using generic."
		eerror ""
		newins make.conf make.conf.example
	fi

	dosym ..${portage_share_config}/make.globals /etc/make.globals

	insinto /etc/logrotate.d
	doins "${S}"/cnf/logrotate.d/elog-save-summary

	dodir ${portage_base}/bin
	exeinto ${portage_base}/bin

	# BSD and OSX need a sed wrapper so that find/xargs work properly
	if use userland_GNU; then
		rm "${S}"/bin/sed || die "Failed to remove sed wrapper"
	fi

	cd "${S}"/bin
	doexe *
	dosym newins ${portage_base}/bin/donewins

	local symlinks
	for mydir in $(find "${S}"/pym -type d | sed -e "s:^${S}/::") ; do
		dodir ${portage_base}/${mydir}
		insinto ${portage_base}/${mydir}
		cd "${S}"/${mydir}
		doins *.py
		symlinks=$(find . -mindepth 1 -maxdepth 1 -type l)
		[ -n "${symlinks}" ] && cp -P ${symlinks} "${D}${portage_base}/${mydir}"
	done

	# Symlinks to directories cause up/downgrade issues and the use of these
	# modules outside of portage is probably negligible.
	for x in "${D}${portage_base}/pym/"{cache,elog_modules} ; do
		[ ! -L "${x}" ] && continue
		die "symlink to directory will cause upgrade/downgrade issues: '${x}'"
	done

	exeinto ${portage_base}/pym/portage/tests
	doexe  "${S}"/pym/portage/tests/runTests

	doman "${S}"/man/*.[0-9]
	if use linguas_pl; then
		doman -i18n=pl "${S_PL}"/man/pl/*.[0-9]
		doman -i18n=pl_PL.UTF-8 "${S_PL}"/man/pl_PL.UTF-8/*.[0-9]
	fi

	dodoc "${S}"/{ChangeLog,NEWS,RELEASE-NOTES}
	use doc && dohtml -r "${S}"/doc/*
	use epydoc && dohtml -r "${WORKDIR}"/api

	dodir /usr/bin
	for x in ebuild emerge portageq repoman xpak; do
		dosym ../${libdir}/portage/bin/${x} /usr/bin/${x}
	done

	dodir /usr/sbin
	local x
	for x in archive-conf dispatch-conf emaint emerge-webrsync env-update \
		etc-update fixpackages quickpkg regenworld ; do
		dosym ../${libdir}/portage/bin/${x} /usr/sbin/${x}
	done
	dosym env-update /usr/sbin/update-env
	dosym etc-update /usr/sbin/update-etc

	dodir /etc/portage
	keepdir /etc/portage
}

pkg_preinst() {
	if [ -f "${ROOT}/etc/make.globals" ]; then
		rm "${ROOT}/etc/make.globals"
	fi
	has_version ">=${CATEGORY}/${PN}-2.2_pre"
	DOWNGRADE_FROM_2_2=$?
	has_version "<${CATEGORY}/${PN}-2.1.6_pre"
	UPGRADE_FROM_2_1=$?
}

pkg_postinst() {
	# Compile all source files recursively. Any orphans
	# will be identified and removed in postrm.
	python_mod_optimize /usr/$(get_libdir)/portage/pym

	local warning_shown=0
	if [ $DOWNGRADE_FROM_2_2 = 0 ] ; then
		ewarn
		echo "Since you have downgraded from portage-2.2, do not forget to" \
		"use revdep-rebuild when appropriate, since the @preserved-rebuild" \
		"package set is only supported with portage-2.2." | fmt -w 70 | \
		while read ; do ewarn "$REPLY" ; done
		warning_shown=1
	fi
	if [ $UPGRADE_FROM_2_1 = 0 ] ; then
		ewarn
		echo "In portage-2.1.6, the default behavior has changed for" \
		"\`emerge world\` and \`emerge system\` commands. These commands" \
		"will reinstall all packages from the given set unless an option" \
		"such as --noreplace, --update, or --newuse is specified." \
		| fmt -w 70 | while read ; do ewarn "$REPLY" ; done
		ewarn
		echo "File collision protection is now enabled by default via" \
		"make.globals with FEATURES=protect-owned. If you want to" \
		"disable collision protection completely (not recommended), then" \
		"you need to ensure that neither protect-owned nor collision-protect" \
		"are enabled." | fmt -w 70 | while read ; do ewarn "$REPLY" ; done
		ewarn
		echo "If you have overridden FETCHCOMMAND or RESUMECOMMAND variables," \
		"for compatibility with EAPI 2, you must ensure that these variables" \
		"are written such that the downloaded file will be placed at" \
		"\"\${DISTDIR}/\${FILE}\". Refer to make.conf(5) for" \
		"information about FETCHCOMMAND and RESUMECOMMAND." | \
		fmt -w 70 | while read ; do ewarn "$REPLY" ; done
		warning_shown=1
	fi
	if [ $warning_shown = 1 ] ; then
		ewarn # for symmetry
	fi
}

pkg_postrm() {
	python_mod_cleanup /usr/$(get_libdir)/portage/pym
}
