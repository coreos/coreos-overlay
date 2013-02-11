# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/readme.gentoo.eclass,v 1.2 2013/01/24 21:38:41 pacho Exp $

# @ECLASS: readme.gentoo
# @MAINTAINER:
# Pacho Ramos <pacho@gentoo.org>
# @AUTHOR:
# Author: Pacho Ramos <pacho@gentoo.org>
# @BLURB: An eclass for installing a README.gentoo doc file recording tips
# shown via elog messages.
# @DESCRIPTION:
# An eclass for installing a README.gentoo doc file recording tips           
# shown via elog messages. With this eclass, those elog messages will only be
# shown at first package installation and a file for later reviewing will be
# installed under /usr/share/doc/${PF}

if [[ ${___ECLASS_ONCE_README_GENTOO} != "recur -_+^+_- spank" ]] ; then
___ECLASS_ONCE_README_GENTOO="recur -_+^+_- spank"

inherit eutils

case "${EAPI:-0}" in
	0|1|2|3)
		die "Unsupported EAPI=${EAPI:-0} (too old) for ${ECLASS}"
		;;
	4|5)
		# EAPI>=4 is required for REPLACING_VERSIONS preventing us
		# from needing to export another pkg_preinst phase to save has_version
		# result. Also relies on EAPI >=4 default src_install phase.
		;;
	*)
		die "Unsupported EAPI=${EAPI} (unknown) for ${ECLASS}"
		;;
esac

EXPORT_FUNCTIONS src_install pkg_postinst

# @ECLASS-VARIABLE: FORCE_PRINT_ELOG
# @DEFAULT_UNSET
# @DESCRIPTION:
# If non-empty this variable forces elog messages to be printed.

# @FUNCTION: readme.gentoo_create_doc
# @DESCRIPTION:
# Create doc file with ${DOC_CONTENTS} variable (preferred) and, if not set,
# look for "${FILESDIR}/README.gentoo" contents. You can use 
# ${FILESDIR}/README.gentoo-${SLOT} also.
# Usually called at src_install phase.
readme.gentoo_create_doc() {
	debug-print-function ${FUNCNAME} "${@}"

	if [[ -n "${DOC_CONTENTS}" ]]; then
		eshopts_push
		set -f
		echo ${DOC_CONTENTS} | fmt > "${T}"/README.gentoo
		eshopts_pop
		dodoc "${T}"/README.gentoo
	else
		if [[ -f "${FILESDIR}/README.gentoo-${SLOT%/*}" ]]; then
			cp "${FILESDIR}/README.gentoo-${SLOT%/*}" "${T}"/README.gentoo
			dodoc "${T}"/README.gentoo
		else
			if [[ -f "${FILESDIR}/README.gentoo" ]]; then
				cp "${FILESDIR}/README.gentoo" "${T}"/README.gentoo
				dodoc "${T}"/README.gentoo
			else
				die "You are not specifying README.gentoo contents!"
			fi
		fi
	fi
}

# @FUNCTION: readme.gentoo_print_elog
# @DESCRIPTION:
# Print elog messages with "${T}"/README.gentoo contents. They will be
# shown only when package is installed at first time.
# Usually called at pkg_postinst phase.
#
# If you want to show them always, please set FORCE_PRINT_ELOG to a non empty
# value in your ebuild before this function is called.
# This can be useful when, for example, DOC_CONTENTS is modified, then, you can
# rely on specific REPLACING_VERSIONS handling in your ebuild to print messages
# when people update from versions still providing old message.
readme.gentoo_print_elog() {
	debug-print-function ${FUNCNAME} "${@}"

	if [[ -f "${T}"/README.gentoo ]]; then
		if ! [[ -n "${REPLACING_VERSIONS}" ]] || [[ -n "${FORCE_PRINT_ELOG}" ]]; then
			eshopts_push
			set -f
			cat "${T}"/README.gentoo | while read -r ELINE; do elog "${ELINE}"; done
			eshopts_pop
		fi
	else
		die "README.gentoo wasn't created at src_install!"
	fi
}


# @FUNCTION: readme.gentoo_src_install
# @DESCRIPTION:
# Install generated doc file automatically.
readme.gentoo_src_install() {
	debug-print-function ${FUNCNAME} "${@}"
	default
	readme.gentoo_create_doc
}

# @FUNCTION: readme.gentoo_pkg_postinst
# @DESCRIPTION:
# Show elog messages from from just generated doc file.
readme.gentoo_pkg_postinst() {
	debug-print-function ${FUNCNAME} "${@}"
	readme.gentoo_print_elog
}

fi
