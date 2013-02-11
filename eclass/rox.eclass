# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/rox.eclass,v 1.36 2012/05/02 18:31:42 jdhore Exp $

# ROX eclass Version 3

# This eclass was created by Sergey Kuleshov (svyatogor@gentoo.org) and
# Alexander Simonov (devil@gentoo.org.ua) to ease installation of ROX desktop
# applications. Enhancements and python additions by Peter Hyman.
# Small fixes and current maintenance by Jim Ramsay (lack@gentoo.org)

# These variables are used in the GLOBAL scope to decide on DEPENDs, so they
# must be set BEFORE you 'inherit rox':
#
# ROX_VER - the minimum version of rox filer required. Default is 2.1.0
# ROX_LIB_VER - version of rox-lib required if any
# ROX_CLIB_VER - version of rox-clib required if any
#
# These variables are only used inside functions, and so may be set anywhere in
# the ebuild:
#
# APPNAME - the actual name of the application as the app folder is named
# WRAPPERNAME - the name of the wrapper installed into /usr/bin
#    Defaults to 'rox-${PN}', or just ${PN} if it already starts with 'rox'.
#    This does not normally need to be overridden.
#    If overridden with the reserved word 'skip' no wrapper will be created.
# APPNAME_COLLISION - If not set, the old naming convention for wrappers of
#    /usr/bin/${APPNAME} will still be around.  Needs only be set in packages
#    with known collisions (such as Pager, which collides with afterstep)
# APPCATEGORY - the .desktop categories this application should be placed in.
#    If unset, no .desktop file will be created.  For a list of acceptable
#    category names, see
#    http://standards.freedesktop.org/menu-spec/latest/apa.html
# APPMIME - the .desktop MimeTypes value to include.  This is a
#    semicolon-delimited list of mime-types.  Any characters in [[:space:]] are
#    ignored.  If the supported mime-types are dependent upon USE flags, the new
#    'usemime' function may be useful.  For example:
#       APPMIME="a/foo-1;a/foo-2
#                $(usemime three "a/three")
#                text/plain"
#    will be expanded to either "a/foo-1;a/foo-2;a/three;text/plain" if
#    USE=three or "a/foo-1;a/foo-2;text/plain" if not.
#    WARNING: the 'usemime' function cannot be used in global scope. You should
#    set APPMIME (or at least the USE-dependant parts) in your own src_install
#    before calling rox_src_install.  See rox-extra/archive for an example.
# KEEP_SRC - this flag, if set, will not remove the source directory
#    but will do a 'make clean' in it. This is useful if users wish to
#    preserve the source code for some reason

# For examples refer to ebuilds in rox-extra/ or rox-base/

if [[ ${ROX_LIB_VER} ]]; then
	# Presently all packages which require ROX_LIB also require python2
	PYTHON_DEPEND="2"
fi

# need python to byte compile modules, if any
# need autotools to run autoreconf, if required
inherit python autotools eutils multilib

ROX_VER=${ROX_VER:-"2.1.0"}

RDEPEND=">=rox-base/rox-${ROX_VER}"

if [[ ${ROX_LIB_VER} ]]; then
	RDEPEND="${RDEPEND}
		>=rox-base/rox-lib-${ROX_LIB_VER}"
fi

if [[ ${ROX_CLIB_VER} ]]; then
	RDEPEND="${RDEPEND}
		>=rox-base/rox-clib-${ROX_CLIB_VER}"
	DEPEND="${DEPEND}
		>=rox-base/rox-clib-${ROX_CLIB_VER}
		virtual/pkgconfig"
fi

# This is the new wrapper name (for /usr/bin/)
#   It is also used for the icon name in /usr/share/pixmaps
#
# Use rox-${PN} unless ${PN} already starts with 'rox'
_a="rox-${PN}"
_b=${_a/rox-rox*}
WRAPPERNAME=${_b:-${PN}}
unset _a _b

# This is the location where all applications are installed
LIBDIR="/usr/$(get_libdir)"
APPDIR="${LIBDIR}/rox"

# External Functions

# Used for assembling APPMIME with USE-dependent parts
# WARNING: Cannot be used in global scope.
#          Set this in src_install just before you call rox_src_install
usemime() {
	local myuse="$1"; shift
	use "${myuse}" && echo "$@"
}

# Utility Functions

# Expands APPMIME properly, removing whitespace, newlines, and putting in ';'
# where needed.
expandmime() {
	local old_IFS=$IFS
	IFS=$'; \t\n'
	echo "$*"
	IFS=$old_IFS
}

#
# Install the wrapper in /usr/bin for commandline execution
#
# Environment needed:
#   WRAPPERNAME, APPDIR, LIBDIR, APPNAME, APPNAME_COLLISION
#
rox_install_wrapper() {
	if [[ "${WRAPPERNAME}" != "skip" ]]; then
		#create a script in bin to run the application from command line
		dodir /usr/bin/
		cat >"${D}/usr/bin/${WRAPPERNAME}" <<EOF
#!/bin/sh
if [ "\${LIBDIRPATH}" ]; then
	export LIBDIRPATH="\${LIBDIRPATH}:${LIBDIR}"
else
	export LIBDIRPATH="${LIBDIR}"
fi

if [ "\${APPDIRPATH}" ]; then
	export APPDIRPATH="\${APPDIRPATH}:${APPDIR}"
else
	export APPDIRPATH="${APPDIR}"
fi
exec "${APPDIR}/${APPNAME}/AppRun" "\$@"
EOF
		chmod 755 "${D}/usr/bin/${WRAPPERNAME}"

		# Old name of cmdline wrapper: /usr/bin/${APPNAME}
		if [[ ! "${APPNAME_COLLISION}" ]]; then
			ln -s ${WRAPPERNAME} "${D}/usr/bin/${APPNAME}"
			# TODO: Migrate this away... eventually
		else
			ewarn "The wrapper script /usr/bin/${APPNAME} has been removed"
			ewarn "due to a name collision.  You must run ${APPNAME} as"
			ewarn "/usr/bin/${WRAPPERNAME} instead."
		fi
	fi
}

#
# Copy the .DirIcon into the pixmaps dir, and create the .desktop file
#
rox_install_desktop() {
	# Create a .desktop file if the proper category is supplied
	if [[ -n "${APPCATEGORY}" ]]; then
		# Copy the .DirIcon into /usr/share/pixmaps with the proper extension
		if [[ -f "${APPNAME}/.DirIcon" ]]; then
			local APPDIRICON=${APPNAME}/.DirIcon
			local APPICON
			case "$(file -b ${APPDIRICON})" in
				"PNG image"*)
					APPICON=${WRAPPERNAME}.png
					;;
				"XML 1.0 document text"* | \
				"SVG Scalable Vector Graphics image"*)
					APPICON=${WRAPPERNAME}.svg
					;;
				"X pixmap image text"*)
					APPICON=${WRAPPERNAME}.xpm
					;;
				"symbolic link"*)
					APPDIRICON=$(dirname ${APPDIRICON})/$(readlink ${APPDIRICON})
					APPICON=${WRAPPERNAME}.${APPDIRICON##*.}
					;;
				*)
					# Unknown... Remark on it, and just copy without an extension
					ewarn "Could not detect the file type of the application icon,"
					ewarn "copying without an extension."
					APPICON=${WRAPPERNAME}
					;;
			esac
			# Subshell, so as to not pollute the caller's env.
			(
			insinto /usr/share/pixmaps
			newins "${APPDIRICON}" "${APPICON}"
			)
		fi

		make_desktop_entry "${WRAPPERNAME}" "${APPNAME}" "${WRAPPERNAME}" \
			"${APPCATEGORY}" "MimeType=$(expandmime $APPMIME)"
	fi
}

# Exported functions

rox_pkg_setup() {
	if [[ ${PYTHON_DEPEND} ]]; then
		python_set_active_version 2
	fi
}

rox_src_compile() {
	cd "${APPNAME}"
	# Python packages need their shebangs fixed
	if [[ ${PYTHON_DEPEND} ]]; then
		python_convert_shebangs -r 2 .
	fi
	#Some packages need to be compiled.
	chmod 755 AppRun
	if [[ -d src/ ]]; then
		# Bug 150303: Check with Rox-Clib will fail if the user has 0install
		# installed on their system somewhere, so remove the check for it in the
		# configure script, and adjust the path that the 'libdir' program uses
		# to search for it:
		if [[ -f src/configure.in ]]; then
			cd src
			sed -i.bak -e 's/ROX_CLIB_0LAUNCH/ROX_CLIB/' configure.in
			# TODO: This should really be 'eautoreconf', but that breaks
			# some packages (such as rox-base/pager-1.0.1)
			eautoconf
			cd ..
		fi
		export LIBDIRPATH="${LIBDIR}"

		# Most rox self-compiles have a 'read' call to wait for the user to
		# press return if the compile fails.
		# Find and remove this:
		sed -i.bak -e 's/\<read\>/#read/' AppRun

		./AppRun --compile || die "Failed to compile the package"
		if [[ -n "${KEEP_SRC}" ]]; then
			emake -C src clean
		else
			rm -rf src
		fi
		if [[ -d build ]]; then
			rm -rf build
		fi

		# Restore the original AppRun
		mv AppRun.bak AppRun
	fi
}

rox_src_install() {
	if [[ -d "${APPNAME}/Help/" ]]; then
		for i in "${APPNAME}"/Help/*; do
			dodoc "${i}"
		done
	fi

	insinto "${APPDIR}"

	# Use 'cp -pPR' and not 'doins -r' here so we don't have to do a flurry of
	# 'chmod' calls on the executables in the appdir - Just be sure that all the
	# files in the original appdir prior to this step are correct, as they will
	# all be preserved.
	cp -pPR ${APPNAME} "${D}${APPDIR}/${APPNAME}"

	# Install the wrapper in /usr/bin
	rox_install_wrapper

	# Install the .desktop application file
	rox_install_desktop
}

rox_pkg_postinst() {
	if [[ ${PYTHON_DEPEND} ]]; then
		python_mod_optimize "${APPDIR}/${APPNAME}" >/dev/null 2>&1
	fi

	einfo "${APPNAME} has been installed into ${APPDIR}"
	if [[ "${WRAPPERNAME}" != "skip" ]]; then
		einfo "You can run it by typing ${WRAPPERNAME} at the command line."
		einfo "Or, you can run it by pointing the ROX file manager to the"
	else
		einfo "You can run it by pointing the ROX file manager to the"
	fi
	einfo "install location -- ${APPDIR} -- and click"
	einfo "on ${APPNAME}'s icon, drag it to a panel, desktop, etc."
}

rox_pkg_postrm() {
	if [[ ${PYTHON_DEPEND} ]]; then
		python_mod_cleanup "${APPDIR}"
	fi
}


EXPORT_FUNCTIONS pkg_setup src_compile src_install pkg_postinst pkg_postrm
