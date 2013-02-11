# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/pax-utils.eclass,v 1.18 2012/04/06 18:03:54 blueness Exp $

# @ECLASS: pax-utils.eclass
# @MAINTAINER:
# The Gentoo Linux Hardened Team <hardened@gentoo.org>
# @AUTHOR:
# Original Author: Kevin F. Quinn <kevquinn@gentoo.org>
# Modifications for bug #365825, @ ECLASS markup: Anthony G. Basile <blueness@gentoo.org>
# @BLURB: functions to provide pax markings
# @DESCRIPTION:
# This eclass provides support for manipulating PaX markings on ELF binaries,
# wrapping the use of the paxctl and scanelf utilities.  It decides which to
# use depending on what is installed on the build host, preferring paxctl to
# scanelf.  If paxctl is not installed, we fall back to scanelf since it is
# always present.  However, currently scanelf doesn't do all that paxctl can.
#
# To control what markings are made, set PAX_MARKINGS in /etc/make.conf to
# contain either "PT" or "none".  If PAX_MARKINGS is set to "PT", and the
# necessary utility is installed, the PT_PAX_FLAGS markings will be made.  If
# PAX_MARKINGS is set to "none", no markings will be made.

if [[ ${___ECLASS_ONCE_PAX_UTILS} != "recur -_+^+_- spank" ]] ; then
___ECLASS_ONCE_PAX_UTILS="recur -_+^+_- spank"

# Default to PT markings.
PAX_MARKINGS=${PAX_MARKINGS:="PT"}

# @FUNCTION: pax-mark
# @USAGE: <flags> {<ELF files>}
# @RETURN: Shell true if we succeed, shell false otherwise
# @DESCRIPTION:
# Marks <ELF files> with provided PaX <flags>
#
# Flags are passed directly to the utilities unchanged.  Possible flags at the
# time of writing, taken from /sbin/paxctl, are:
#
#	p: disable PAGEEXEC		P: enable PAGEEXEC
#	e: disable EMUTRMAP		E: enable EMUTRMAP
#	m: disable MPROTECT		M: enable MPROTECT
#	r: disable RANDMMAP		R: enable RANDMMAP
#	s: disable SEGMEXEC		S: enable SEGMEXEC
#
# Default flags are 'PeMRS', which are the most restrictive settings.  Refer
# to http://pax.grsecurity.net/ for details on what these flags are all about.
# Do not use the obsolete flag 'x'/'X' which has been deprecated.
#
# Please confirm any relaxation of restrictions with the Gentoo Hardened team.
# Either ask on the gentoo-hardened mailing list, or CC/assign hardened@g.o on
# the bug report.
pax-mark() {
	local f flags fail=0 failures="" zero_load_alignment
	# Ignore '-' characters - in particular so that it doesn't matter if
	# the caller prefixes with -
	flags=${1//-}
	shift
	# Try paxctl, then scanelf.  paxctl is preferred.
	if type -p paxctl > /dev/null && has PT ${PAX_MARKINGS}; then
		# Try paxctl, the upstream supported tool.
		einfo "PT PaX marking -${flags}"
		_pax_list_files einfo "$@"
		for f in "$@"; do
			# First, try modifying the existing PAX_FLAGS header
			paxctl -q${flags} "${f}" && continue
			# Second, try stealing the (unused under PaX) PT_GNU_STACK header
			paxctl -qc${flags} "${f}" && continue
			# Third, try pulling the base down a page, to create space and
			# insert a PT_GNU_STACK header (works on ET_EXEC)
			paxctl -qC${flags} "${f}" && continue
			#
			# prelink is masked on hardened so we wont use this method.
			# We're working on a new utiity to try to do the same safely. See
			# http://git.overlays.gentoo.org/gitweb/?p=proj/elfix.git;a=summary
			#
			# Fourth - check if it loads to 0 (probably an ET_DYN) and if so,
			# try rebasing with prelink first to give paxctl some space to
			# grow downwards into.
			#if type -p objdump > /dev/null && type -p prelink > /dev/null; then
			#	zero_load_alignment=$(objdump -p "${f}" | \
			#		grep -E '^[[:space:]]*LOAD[[:space:]]*off[[:space:]]*0x0+[[:space:]]' | \
			#		sed -e 's/.*align\(.*\)/\1/')
			#	if [[ ${zero_load_alignment} != "" ]]; then
			#		prelink -r $(( 2*(${zero_load_alignment}) )) &&
			#		paxctl -qC${flags} "${f}" && continue
			#	fi
			#fi
			fail=1
			failures="${failures} ${f}"
		done
	elif type -p scanelf > /dev/null && [[ ${PAX_MARKINGS} != "none" ]]; then
		# Try scanelf, the Gentoo swiss-army knife ELF utility
		# Currently this sets PT if it can, no option to control what it does.
		einfo "Fallback PaX marking -${flags}"
		_pax_list_files einfo "$@"
		scanelf -Xxz ${flags} "$@"
	elif [[ ${PAX_MARKINGS} != "none" ]]; then
		# Out of options!
		failures="$*"
		fail=1
	fi
	if [[ ${fail} == 1 ]]; then
		ewarn "Failed to set PaX markings -${flags} for:"
		_pax_list_files ewarn ${failures}
		ewarn "Executables may be killed by PaX kernels."
	fi
	return ${fail}
}

# @FUNCTION: list-paxables
# @USAGE: {<files>}
# @RETURN: Subset of {<files>} which are ELF executables or shared objects
# @DESCRIPTION:
# Print to stdout all of the <files> that are suitable to have PaX flag
# markings, i.e., filter out the ELF executables or shared objects from a list
# of files.  This is useful for passing wild-card lists to pax-mark, although
# in general it is preferable for ebuilds to list precisely which ELFS are to
# be marked.  Often not all the ELF installed by a package need remarking.
# @EXAMPLE:
# pax-mark -m $(list-paxables ${S}/{,usr/}bin/*)
list-paxables() {
	file "$@" 2> /dev/null | grep -E 'ELF.*(executable|shared object)' | sed -e 's/: .*$//'
}

# @FUNCTION: host-is-pax
# @RETURN: Shell true if the build process is PaX enabled, shell false otherwise
# @DESCRIPTION:
# This is intended for use where the build process must be modified conditionally
# depending on whether the host is PaX enabled or not.  It is not intedened to
# determine whether the final binaries need PaX markings.  Note: if procfs is
# not mounted on /proc, this returns shell false (e.g. Gentoo/FBSD).
host-is-pax() {
	grep -qs ^PaX: /proc/self/status
}


# INTERNAL FUNCTIONS
# ------------------
#
# These functions are for use internally by the eclass - do not use
# them elsewhere as they are not supported (i.e. they may be removed
# or their function may change arbitratily).

# Display a list of things, one per line, indented a bit, using the
# display command in $1.
_pax_list_files() {
	local f cmd
	cmd=$1
	shift
	for f in "$@"; do
		${cmd} "     ${f}"
	done
}

fi
