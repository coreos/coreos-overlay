# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/office-ext.eclass,v 1.7 2012/09/27 16:35:41 axs Exp $

# @ECLASS: office-ext.eclass
# @MAINTAINER:
# The office team <openoffice@gentoo.org>
# @AUTHOR:
# Tomáš Chvátal <scarabeus@gentoo.org>
# @BLURB: Eclass for installing libreoffice/openoffice extensions
# @DESCRIPTION:
# Eclass for easing maitenance of libreoffice/openoffice extensions.

case "${EAPI:-0}" in
	4|5) OEXT_EXPORTED_FUNCTIONS="src_unpack src_install pkg_postinst pkg_prerm" ;;
	*) die "EAPI=${EAPI} is not supported" ;;
esac

inherit eutils multilib

UNOPKG_BINARY="${EPREFIX}/usr/bin/unopkg"

# @ECLASS-VARIABLE: OO_EXTENSIONS
# @REQUIRED
# @DESCRIPTION:
# Array containing list of extensions to install.
[[ -z ${OO_EXTENSIONS} ]] && die "OO_EXTENSIONS variable is unset."
if [[ "$(declare -p OO_EXTENSIONS 2>/dev/null 2>&1)" != "declare -a"* ]]; then
	die "OO_EXTENSIONS variable is not an array."
fi

DEPEND="virtual/ooo"
RDEPEND="virtual/ooo"

# Most projects actually do not provide any workdir and we do not unpack the
# .oxt file at all.
S="${WORKDIR}"

# @FUNCTION: office-ext_src_unpack
# @DESCRIPTION:
# Flush the cache after removal of an extension.
office-ext_src_unpack() {
	debug-print-function ${FUNCNAME} "$@"
	local i

	default

	for i in ${OO_EXTENSIONS[@]}; do
		# Copy only if the oxt is present, sometimes they are in tarballs
		# so we want to fail only when we do need to do the cp.
		if [[ -f "${DISTDIR}/${i}" ]] ; then
			debug-print "${FUNCNAME}: cp -v \"${DISTDIR}/${i}\" \"${S}\""
			cp -v "${DISTDIR}/${i}" "${S}" || die
		fi
	done
}

# @FUNCTION: office-ext_flush_unopkg_cache
# @DESCRIPTION:
# Flush the cache after removal of an extension.
office-ext_flush_unopkg_cache() {
	debug-print-function ${FUNCNAME} "$@"

	debug-print "${FUNCNAME}: ${UNOPKG_BINARY} list --shared > /dev/null"
	${UNOPKG_BINARY} list --shared > /dev/null
}

# @FUNCTION: office-ext_get_implementation
# @DESCRIPTION:
# Determine the implementation we are building against.
office-ext_get_implementation() {
	debug-print-function ${FUNCNAME} "$@"
	local implementations=(
		"libreoffice"
		"openoffice"
	)
	local i

	for i in "${implementations[@]}"; do
		if [[ -d "${EPREFIX}/usr/$(get_libdir)/${i}" ]]; then
			debug-print "${FUNCNAME}: Determined implementation is: \"${EPREFIX}/usr/$(get_libdir)/${i}\""
			echo "${EPREFIX}/usr/$(get_libdir)/${i}"
			return
		fi
	done

	die "Unable to determine libreoffice/openoffice implementation!"
}

# @FUNCTION: office-ext_add_extension
# @DESCRIPTION:
# Install the extension into the libreoffice/openoffice.
office-ext_add_extension() {
	debug-print-function ${FUNCNAME} "$@"
	local ext=$1
	local tmpdir=$(mktemp -d --tmpdir="${T}")

	debug-print "${FUNCNAME}: ${UNOPKG_BINARY} add --shared \"${ext}\""
	ebegin "Adding office extension: \"${ext}\""
	${UNOPKG_BINARY} add --suppress-license \
		--shared "${ext}" \
		"-env:UserInstallation=file:///${tmpdir}" \
		"-env:JFW_PLUGIN_DO_NOT_CHECK_ACCESSIBILITY=1"
	eend $?
	rm -rf "${tmpdir}"
}

# @FUNCTION: office-ext_remove_extension
# @DESCRIPTION:
# Remove the extension from the libreoffice/openoffice.
office-ext_remove_extension() {
	debug-print-function ${FUNCNAME} "$@"
	local ext=$1
	local tmpdir=$(mktemp -d --tmpdir="${T}")

	debug-print "${FUNCNAME}: ${UNOPKG_BINARY} remove --shared \"${ext}\""
	ebegin "Removing office extension: \"${ext}\""
	${UNOPKG_BINARY} remove --suppress-license \
		--shared "${ext}" \
		"-env:UserInstallation=file:///${tmpdir}" \
		"-env:JFW_PLUGIN_DO_NOT_CHECK_ACCESSIBILITY=1"
	eend $?
	office-ext_flush_unopkg_cache
	rm -rf "${tmpdir}"
}

# @FUNCTION: office-ext_src_install
# @DESCRIPTION:
# Install the extension source to the proper location.
office-ext_src_install() {
	debug-print-function ${FUNCNAME} "$@"
	debug-print "Extensions: ${OO_EXTENSIONS[@]}"
	local i

	# subshell to not pollute rest of the env with the insinto redefinition
	(
		dodir $(office-ext_get_implementation)/share/extension/install/
		insinto $(office-ext_get_implementation)/share/extension/install/
		for i in ${OO_EXTENSIONS[@]}; do
			doins "${i}"
		done
	)

	einfo "Remember that if you replace your office implementation,"
	einfo "you need to recompile all the extensions."
	einfo "Your current implementation location is: "
	einfo "    $(office-ext_get_implementation)"
}

# @FUNCTION: office-ext_pkg_postinst
# @DESCRIPTION:
# Add the extensions to the libreoffice/openoffice.
office-ext_pkg_postinst() {
	debug-print-function ${FUNCNAME} "$@"
	debug-print "Extensions: ${OO_EXTENSIONS[@]}"
	local i

	for i in ${OO_EXTENSIONS[@]}; do
		office-ext_add_extension "$(office-ext_get_implementation)/share/extension/install/${i}"
	done

}

# @FUNCTION: office-ext_pkg_prerm
# @DESCRIPTION:
# Remove the extensions from the libreoffice/openoffice.
office-ext_pkg_prerm() {
	debug-print-function ${FUNCNAME} "$@"
	debug-print "Extensions: ${OO_EXTENSIONS[@]}"
	local i

	for i in ${OO_EXTENSIONS[@]}; do
		office-ext_remove_extension "${i}"
	done
}

EXPORT_FUNCTIONS ${OEXT_EXPORTED_FUNCTIONS}
unset OEXT_EXPORTED_FUNCTIONS
