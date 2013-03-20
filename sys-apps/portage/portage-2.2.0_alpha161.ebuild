# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/portage/portage-2.2.0_alpha161.ebuild,v 1.2 2013/01/27 22:04:14 zmedico Exp $

# Require EAPI 2 since we now require at least python-2.6 (for python 3
# syntax support) which also requires EAPI 2.
EAPI=3
inherit eutils python

DESCRIPTION="Portage is the package management and distribution system for Gentoo"
HOMEPAGE="http://www.gentoo.org/proj/en/portage/index.xml"
LICENSE="GPL-2"
KEYWORDS="~amd64-fbsd ~sparc-fbsd ~x86-fbsd"
SLOT="0"
IUSE="build doc epydoc +ipc linguas_pl linguas_ru pypy2_0 python2 python3 selinux xattr"

# Import of the io module in python-2.6 raises ImportError for the
# thread module if threading is disabled.
python_dep_ssl="python3? ( =dev-lang/python-3*[ssl] )
	!pypy2_0? ( !python2? ( !python3? (
		|| ( >=dev-lang/python-2.7[ssl] dev-lang/python:2.6[threads,ssl] )
	) ) )
	pypy2_0? ( !python2? ( !python3? ( dev-python/pypy:2.0[bzip2,ssl] ) ) )
	python2? ( !python3? ( || ( dev-lang/python:2.7[ssl] dev-lang/python:2.6[ssl,threads] ) ) )"
python_dep="${python_dep_ssl//\[ssl\]}"
python_dep="${python_dep//,ssl}"
python_dep="${python_dep//ssl,}"

# The pysqlite blocker is for bug #282760.
DEPEND="${python_dep}
	>=sys-apps/sed-4.0.5 sys-devel/patch
	doc? ( app-text/xmlto ~app-text/docbook-xml-dtd-4.4 )
	epydoc? ( >=dev-python/epydoc-2.0 !<=dev-python/pysqlite-2.4.1 )"
# Require sandbox-2.2 for bug #288863.
# For xattr, we can spawn getfattr and setfattr from sys-apps/attr, but that's
# quite slow, so it's not considered in the dependencies as an alternative to
# to python-3.3 / pyxattr. Also, xattr support is only tested with Linux, so
# for now, don't pull in xattr deps for other kernels.
# For whirlpool hash, require python[ssl] or python-mhash (bug #425046).
# For compgen, require bash[readline] (bug #445576).
RDEPEND="${python_dep}
	!build? ( >=sys-apps/sed-4.0.5
		|| ( >=app-shells/bash-4.2_p37[readline] ( <app-shells/bash-4.2_p37 >=app-shells/bash-3.2_p17 ) )
		>=app-admin/eselect-1.2
		|| ( ${python_dep_ssl} dev-python/python-mhash )
	)
	elibc_FreeBSD? ( sys-freebsd/freebsd-bin )
	elibc_glibc? ( >=sys-apps/sandbox-2.2 )
	elibc_uclibc? ( >=sys-apps/sandbox-2.2 )
	>=app-misc/pax-utils-0.1.17
	xattr? ( kernel_linux? ( || ( >=dev-lang/python-3.3_pre20110902 dev-python/pyxattr ) ) )
	selinux? ( || ( >=sys-libs/libselinux-2.0.94[python] <sys-libs/libselinux-2.0.94 ) )
	!<app-shells/bash-3.2_p17
	!<app-admin/logrotate-3.8.0"
PDEPEND="
	!build? (
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
TARBALL_PV=2.2.0_alpha160
SRC_URI="mirror://gentoo/${PN}-${TARBALL_PV}.tar.bz2
	$(prefix_src_archives ${PN}-${TARBALL_PV}.tar.bz2)
	linguas_pl? ( mirror://gentoo/${PN}-man-pl-${PV_PL}.tar.bz2
		$(prefix_src_archives ${PN}-man-pl-${PV_PL}.tar.bz2) )"

PATCHVER=
[[ $TARBALL_PV = $PV ]] || PATCHVER=$PV
if [ -n "${PATCHVER}" ]; then
	SRC_URI="${SRC_URI} mirror://gentoo/${PN}-${PATCHVER}.patch.bz2
	$(prefix_src_archives ${PN}-${PATCHVER}.patch.bz2)"
fi

S="${WORKDIR}"/${PN}-${TARBALL_PV}
S_PL="${WORKDIR}"/${PN}-${PV_PL}

compatible_python_is_selected() {
	[[ $("${EPREFIX}/usr/bin/python" -c 'import sys ; sys.stdout.write(sys.hexversion >= 0x2060000 and "good" or "bad")') = good ]]
}

current_python_has_xattr() {
	[[ $("${EPREFIX}/usr/bin/python" -c 'import sys ; sys.stdout.write(sys.hexversion >= 0x3030000 and "yes" or "no")') = yes ]] || \
	"${EPREFIX}/usr/bin/python" -c 'import xattr' 2>/dev/null
}

pkg_setup() {
	if use python2 && use python3 ; then
		ewarn "Both python2 and python3 USE flags are enabled, but only one"
		ewarn "can be in the shebangs. Using python3."
	fi
	if use pypy2_0 && use python3 ; then
		ewarn "Both pypy2_0 and python3 USE flags are enabled, but only one"
		ewarn "can be in the shebangs. Using python3."
	fi
	if use pypy2_0 && use python2 ; then
		ewarn "Both pypy2_0 and python2 USE flags are enabled, but only one"
		ewarn "can be in the shebangs. Using python2"
	fi
	if ! use pypy2_0 && ! use python2 && ! use python3 && \
		! compatible_python_is_selected ; then
		ewarn "Attempting to select a compatible default python interpreter"
		local x success=0
		for x in /usr/bin/python2.* ; do
			x=${x#/usr/bin/python2.}
			if [[ $x -ge 6 ]] 2>/dev/null ; then
				eselect python set python2.$x
				if compatible_python_is_selected ; then
					elog "Default python interpreter is now set to python-2.$x"
					success=1
					break
				fi
			fi
		done
		if [ $success != 1 ] ; then
			eerror "Unable to select a compatible default python interpreter!"
			die "This version of portage requires at least python-2.6 to be selected as the default python interpreter (see \`eselect python --help\`)."
		fi
	fi

	if use python3; then
		python_set_active_version 3
	elif use python2; then
		python_set_active_version 2
	elif use pypy2_0; then
		python_set_active_version 2.7-pypy-2.0
	fi
}

src_prepare() {
	if [ -n "${PATCHVER}" ] ; then
		if [[ -L $S/bin/ebuild-helpers/portageq ]] ; then
			rm "$S/bin/ebuild-helpers/portageq" \
				|| die "failed to remove portageq helper symlink"
		fi
		epatch "${WORKDIR}/${PN}-${PATCHVER}.patch"
	fi
	einfo "Setting portage.VERSION to ${PVR} ..."
	sed -e "s/^VERSION=.*/VERSION=\"${PVR}\"/" -i pym/portage/__init__.py || \
		die "Failed to patch portage.VERSION"
	sed -e "1s/VERSION/${PVR}/" -i doc/fragment/version || \
		die "Failed to patch VERSION in doc/fragment/version"
	sed -e "1s/VERSION/${PVR}/" -i $(find man -type f) || \
		die "Failed to patch VERSION in man page headers"

	if ! use ipc ; then
		einfo "Disabling ipc..."
		sed -e "s:_enable_ipc_daemon = True:_enable_ipc_daemon = False:" \
			-i pym/_emerge/AbstractEbuildProcess.py || \
			die "failed to patch AbstractEbuildProcess.py"
	fi

	if use xattr && use kernel_linux ; then
		einfo "Adding FEATURES=xattr to make.globals ..."
		echo -e '\nFEATURES="${FEATURES} xattr"' >> cnf/make.globals \
			|| die "failed to append to make.globals"
	fi

	if use python3; then
		einfo "Converting shebangs for python3..."
		python_convert_shebangs -r 3 .
	elif use python2; then
		einfo "Converting shebangs for python2..."
		python_convert_shebangs -r 2 .
	elif use pypy2_0; then
		einfo "Converting shebangs for pypy-c2.0..."
		python_convert_shebangs -r 2.7-pypy-2.0 .
	fi

	if [[ -n ${EPREFIX} ]] ; then
		einfo "Setting portage.const.EPREFIX ..."
		sed -e "s|^\(SANDBOX_BINARY[[:space:]]*=[[:space:]]*\"\)\(/usr/bin/sandbox\"\)|\\1${EPREFIX}\\2|" \
			-e "s|^\(FAKEROOT_BINARY[[:space:]]*=[[:space:]]*\"\)\(/usr/bin/fakeroot\"\)|\\1${EPREFIX}\\2|" \
			-e "s|^\(BASH_BINARY[[:space:]]*=[[:space:]]*\"\)\(/bin/bash\"\)|\\1${EPREFIX}\\2|" \
			-e "s|^\(MOVE_BINARY[[:space:]]*=[[:space:]]*\"\)\(/bin/mv\"\)|\\1${EPREFIX}\\2|" \
			-e "s|^\(PRELINK_BINARY[[:space:]]*=[[:space:]]*\"\)\(/usr/sbin/prelink\"\)|\\1${EPREFIX}\\2|" \
			-e "s|^\(EPREFIX[[:space:]]*=[[:space:]]*\"\).*|\\1${EPREFIX}\"|" \
			-i pym/portage/const.py || \
			die "Failed to patch portage.const.EPREFIX"

		einfo "Prefixing shebangs ..."
		find . -type f -print0 | \
		while read -r -d $'\0' ; do
			local shebang=$(head -n1 "$REPLY")
			if [[ ${shebang} == "#!"* && ! ${shebang} == "#!${EPREFIX}/"* ]] ; then
				sed -i -e "1s:.*:#!${EPREFIX}${shebang:2}:" "$REPLY" || \
					die "sed failed"
			fi
		done

		einfo "Adjusting make.globals ..."
		sed -e 's|^SYNC=.*|SYNC="rsync://rsync.prefix.freens.org/gentoo-portage-prefix"|' \
			-e "s|^\(PORTDIR=\)\(/usr/portage\)|\\1\"${EPREFIX}\\2\"|" \
			-e "s|^\(PORTAGE_TMPDIR=\)\(/var/tmp\)|\\1\"${EPREFIX}\\2\"|" \
			-i cnf/make.globals || die "sed failed"

		einfo "Adding FEATURES=force-prefix to make.globals ..."
		echo -e '\nFEATURES="${FEATURES} force-prefix"' >> cnf/make.globals \
			|| die "failed to append to make.globals"
	fi

	echo -e '\nFEATURES="${FEATURES} preserve-libs"' >> cnf/make.globals \
		|| die "failed to append to make.globals"

	cd "${S}/cnf" || die
	if [ -f "make.conf.${ARCH}".diff ]; then
		patch make.conf "make.conf.${ARCH}".diff || \
			die "Failed to patch make.conf.example"
	else
		eerror ""
		eerror "Portage does not have an arch-specific configuration for this arch."
		eerror "Please notify the arch maintainer about this issue. Using generic."
		eerror ""
	fi
}

src_compile() {
	if use doc; then
		emake docbook || die
	fi

	if use epydoc; then
		einfo "Generating api docs"
		emake epydoc || die
	fi
}

src_test() {
	# make files executable, in case they were created by patch
	find bin -type f | xargs chmod +x
	emake test || die
}

src_install() {
	emake DESTDIR="${D}" \
		sysconfdir="${EPREFIX}/etc" \
		prefix="${EPREFIX}/usr" \
		install || die

	# Use dodoc for compression, since the Makefile doesn't do that.
	dodoc "${S}"/{ChangeLog,NEWS,RELEASE-NOTES} || die

	if use linguas_pl; then
		doman -i18n=pl "${S_PL}"/man/pl/*.[0-9] || die
		doman -i18n=pl_PL.UTF-8 "${S_PL}"/man/pl_PL.UTF-8/*.[0-9] || die
	fi

	# Set PYTHONPATH for portage API consumers. This way we don't have
	# to rely on patched python having the correct path, since it has
	# been known to incorrectly add /usr/libx32/portage/pym to sys.path.
	echo "PYTHONPATH=\"${EPREFIX}/usr/lib/portage/pym\"" > \
		"${T}/05portage" || die
	doenvd "${T}/05portage" || die
}

pkg_preinst() {
	if [[ $ROOT == / ]] ; then
		# Run some minimal tests as a sanity check.
		local test_runner=$(find "$ED" -name runTests)
		if [[ -n $test_runner && -x $test_runner ]] ; then
			einfo "Running preinst sanity tests..."
			"$test_runner" || die "preinst sanity tests failed"
		fi
	fi

	if use xattr && ! current_python_has_xattr ; then
		ewarn "For optimal performance in xattr handling, install"
		ewarn "dev-python/pyxattr, or install >=dev-lang/python-3.3 and"
		ewarn "enable USE=python3 for $CATEGORY/$PN."
	fi

	has_version "<=${CATEGORY}/${PN}-2.2_pre5" \
		&& WORLD_MIGRATION_UPGRADE=true || WORLD_MIGRATION_UPGRADE=false

	# If portage-2.1.6 is installed and the preserved_libs_registry exists,
	# assume that the NEEDED.ELF.2 files have already been generated.
	has_version "<=${CATEGORY}/${PN}-2.2_pre7" && \
		! ( [ -e "${EROOT}"var/lib/portage/preserved_libs_registry ] && \
		has_version ">=${CATEGORY}/${PN}-2.1.6_rc" ) \
		&& NEEDED_REBUILD_UPGRADE=true || NEEDED_REBUILD_UPGRADE=false
}

pkg_postinst() {
	# Compile all source files recursively. Any orphans
	# will be identified and removed in postrm.
	python_mod_optimize /usr/lib/portage/pym

	if $WORLD_MIGRATION_UPGRADE && \
		grep -q "^@" "${EROOT}/var/lib/portage/world"; then
		einfo "moving set references from the worldfile into world_sets"
		cd "${EROOT}/var/lib/portage/"
		grep "^@" world >> world_sets
		sed -i -e '/^@/d' world
	fi

	if $NEEDED_REBUILD_UPGRADE ; then
		einfo "rebuilding NEEDED.ELF.2 files"
		for cpv in "${EROOT}/var/db/pkg"/*/*; do
			if [ -f "${cpv}/NEEDED" ]; then
				rm -f "${cpv}/NEEDED.ELF.2"
				while read line; do
					filename=${line% *}
					needed=${line#* }
					needed=${needed//+/++}
					needed=${needed//#/##}
					needed=${needed//%/%%}
					newline=$(scanelf -BF "%a;%F;%S;%r;${needed}" $filename)
					newline=${newline//  -  }
					echo "${newline:3}" >> "${cpv}/NEEDED.ELF.2"
				done < "${cpv}/NEEDED"
			fi
		done
	fi
}

pkg_postrm() {
	python_mod_cleanup /usr/lib/portage/pym
}
