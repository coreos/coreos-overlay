# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/bash-completion-r1.eclass,v 1.3 2012/09/27 16:35:41 axs Exp $

# @ECLASS: bash-completion-r1.eclass
# @MAINTAINER:
# mgorny@gentoo.org
# @BLURB: A few quick functions to install bash-completion files
# @EXAMPLE:
#
# @CODE
# EAPI=4
#
# src_install() {
# 	default
#
# 	newbashcomp contrib/${PN}.bash-completion ${PN}
# }
# @CODE

case ${EAPI:-0} in
	0|1|2|3|4|5) ;;
	*) die "EAPI ${EAPI} unsupported (yet)."
esac

# @FUNCTION: dobashcomp
# @USAGE: file [...]
# @DESCRIPTION:
# Install bash-completion files passed as args. Has EAPI-dependant failure
# behavior (like doins).
dobashcomp() {
	debug-print-function ${FUNCNAME} "${@}"

	(
		insinto /usr/share/bash-completion
		doins "${@}"
	)
}

# @FUNCTION: newbashcomp
# @USAGE: file newname
# @DESCRIPTION:
# Install bash-completion file under a new name. Has EAPI-dependant failure
# behavior (like newins).
newbashcomp() {
	debug-print-function ${FUNCNAME} "${@}"

	(
		insinto /usr/share/bash-completion
		newins "${@}"
	)
}
