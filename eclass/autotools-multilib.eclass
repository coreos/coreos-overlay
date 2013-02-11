# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/autotools-multilib.eclass,v 1.8 2013/02/01 21:39:50 mgorny Exp $

# @ECLASS: autotools-multilib.eclass
# @MAINTAINER:
# Michał Górny <mgorny@gentoo.org>
# @BLURB: autotools-utils wrapper for multilib builds
# @DESCRIPTION:
# The autotools-multilib.eclass is an autotools-utils.eclass(5) wrapper
# introducing support for building for more than one ABI (multilib).
#
# Inheriting this eclass sets IUSE=multilib and exports autotools-utils
# phase function wrappers which build the package for each supported ABI
# if the flag is enabled. Otherwise, it works like regular
# autotools-utils.
#
# Note that the multilib support requires out-of-source builds to be
# enabled. Thus, it is impossible to use AUTOTOOLS_IN_SOURCE_BUILD with
# it.

# EAPI=5 is required for meaningful MULTILIB_USEDEP.
case ${EAPI:-0} in
	5) ;;
	*) die "EAPI=${EAPI} is not supported" ;;
esac

if [[ ${AUTOTOOLS_IN_SOURCE_BUILD} ]]; then
	die "${ECLASS}: multilib support requires out-of-source builds."
fi

inherit autotools-utils multilib-build

EXPORT_FUNCTIONS src_configure src_compile src_test src_install

autotools-multilib_src_configure() {
	multilib_parallel_foreach_abi autotools-utils_src_configure
}

autotools-multilib_src_compile() {
	multilib_foreach_abi autotools-utils_src_compile
}

autotools-multilib_src_test() {
	multilib_foreach_abi autotools-utils_src_test
}

autotools-multilib_src_install() {
	autotools-multilib_secure_install() {
		autotools-utils_src_install

		# Make sure all headers are the same for each ABI.
		autotools-multilib_cksum() {
			find "${ED}"usr/include -type f \
				-exec cksum {} + | sort -k2
		}

		local cksum=$(autotools-multilib_cksum)
		local cksum_file=${T}/.autotools-multilib_cksum

		if [[ -f ${cksum_file} ]]; then
			local cksum_prev=$(< "${cksum_file}")

			if [[ ${cksum} != ${cksum_prev} ]]; then
				echo "${cksum}" > "${cksum_file}.new"

				eerror "Header files have changed between ABIs."

				if type -p diff &>/dev/null; then
					eerror "$(diff -du "${cksum_file}" "${cksum_file}.new")"
				else
					eerror "Old checksums in: ${cksum_file}"
					eerror "New checksums in: ${cksum_file}.new"
				fi

				die "Header checksum mismatch, aborting."
			fi
		else
			echo "${cksum}" > "${cksum_file}"
		fi
	}

	multilib_foreach_abi autotools-multilib_secure_install
}
