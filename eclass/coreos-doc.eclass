# Copyright 2015 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2
# $Header: $

# @ECLASS: coreos-doc.eclass
# @BLURB: utility function for including documentation

# @FUNCTION: coreos-dodoc
# @USAGE: [-r] <list of docs>
coreos-dodoc() {
	debug-print-function ${FUNCNAME} "${@}"

	local flags
	if [[ "${1}" == "-r" ]] ; then
		flags="-r"
		shift
	fi

	[[ "${#}" -lt 1 ]] && die "${0}: at least one file needed"

	insinto "/usr/share/coreos/doc/${P}/"
	doins $flags $@
}
