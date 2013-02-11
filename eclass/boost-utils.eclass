# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/boost-utils.eclass,v 1.3 2012/10/07 08:29:49 mgorny Exp $

if [[ ! ${_BOOST_ECLASS} ]]; then

# @ECLASS: boost-utils.eclass
# @MAINTAINER:
# Michał Górny <mgorny@gentoo.org>
# Tiziano Müller <dev-zero@gentoo.org>
# Sebastian Luther <SebastianLuther@gmx.de>
# Arfrever Frehtes Taifersar Arahesis <arfrever.fta@gmail.com>
# @BLURB: helper functions for packages using Boost C++ library
# @DESCRIPTION:
# Helper functions to be used when building packages using the Boost C++
# library collection.
#
# Please note that this eclass does not set the dependencies for you.
# You need to do that yourself.
#
# For cmake & autotools it is usually necessary to set BOOST_ROOT using
# boost-utils_export_root. However, other build system may require more
# hackery or even appending -I$(boost-utils_get_includedir) to CFLAGS
# and -L$(boost-utils_get_libdir) to LDFLAGS.

case ${EAPI:-0} in
	0|1|2|3|4|5) ;;
	*) die "${ECLASS}.eclass API in EAPI ${EAPI} not yet established."
esac

inherit multilib versionator

# @ECLASS-VARIABLE: BOOST_MAX_SLOT
# @DEFAULT_UNSET
# @DESCRIPTION:
# The maximal (newest) boost slot supported by the package. If unset,
# there is no limit (the newest installed version will be used).

# @FUNCTION: boost-utils_get_best_slot
# @DESCRIPTION:
# Get newest installed slot of Boost. If BOOST_MAX_SLOT is set, the version
# returned will be at most in the specified slot.
boost-utils_get_best_slot() {
	local pkg=dev-libs/boost
	[[ ${BOOST_MAX_SLOT} ]] && pkg="<=${pkg}-${BOOST_MAX_SLOT}.9999"

	local cpv=$(best_version ${pkg})
	get_version_component_range 1-2 ${cpv#dev-libs/boost-}
}

# @FUNCTION: boost-utils_get_includedir
# @USAGE: [slot]
# @DESCRIPTION:
# Get the includedir for the given Boost slot. If no slot is given,
# defaults to the newest installed Boost slot (but not newer than
# BOOST_MAX_SLOT if that variable is set).
#
# Outputs the sole path (without -I).
boost-utils_get_includedir() {
	local slot=${1:-$(boost-utils_get_best_slot)}
	has "${EAPI:-0}" 0 1 2 && ! use prefix && EPREFIX=

	echo -n "${EPREFIX}/usr/include/boost-${slot/./_}"
}

# @FUNCTION: boost-utils_get_libdir
# @USAGE: [slot]
# @DESCRIPTION:
# Get the libdir for the given Boost slot. If no slot is given, defaults
# to the newest installed Boost slot (but not newer than BOOST_MAX_SLOT
# if that variable is set).
#
# Outputs the sole path (without -L).
boost-utils_get_libdir() {
	local slot=${1:-$(boost-utils_get_best_slot)}
	has "${EAPI:-0}" 0 1 2 && ! use prefix && EPREFIX=

	echo -n "${EPREFIX}/usr/$(get_libdir)/boost-${slot/./_}"
}

# @FUNCTION: boost-utils_export_root
# @USAGE: [slot]
# @DESCRIPTION:
# Set the BOOST_ROOT variable to includedir for the given Boost slot.
# If no slot is given, defaults to the newest installed Boost slot(but
# not newer than BOOST_MAX_SLOT if that variable is set).
#
# This variable satisfies both cmake and sys-devel/boost-m4 autoconf
# macros.
boost-utils_export_root() {
	export BOOST_ROOT=$(boost-utils_get_includedir "${@}")
}

_BOOST_ECLASS=1
fi # _BOOST_ECLASS
