# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/rox-0install.eclass,v 1.4 2011/12/27 17:55:12 fauli Exp $

# ROX-0install eclass Version 1

# Created by Jim Ramsay (lack@gentoo.org) to ease installation of ROX desktop
# applications and integrate this with zeroinstall-injector
# (http://0install.net)

# These variables are only used inside functions, and so may be set anywhere in
# the ebuild:
#
# ZEROINSTALL_STRIP_REQUIRES - this flag, if set, will force the local
#    zeroinstall feed to have all its 'requires' directives stripped out
# LOCAL_FEED_SRC - The ebuild-supplied native feed, for those packages which do
#    not already contain one.  By default we check for ${APPNAME}.xml and
#    ${APPNAME}/${APPNAME}.xml

# This is an extension of rox.eclass
inherit rox

DEPEND="${DEPEND}
	>=rox-base/zeroinstall-injector-0.31"

# Some locations for ZEROINSTALL
NATIVE_FEED_DIR="/usr/share/0install.net/native_feeds"
ICON_CACHE_DIR="/var/cache/0install.net/interface_icons"

# Does all the 0install local feed magic you could want:
#   - Parses the input file to get the interface URI
#   - Edits the input file and installs it to the final location
#   - Installs a local feed pointer
#
# Environment variables:
#  ZEROINSTALL_STRIP_REQUIRES - If set, strips all 'requires' sections from the XML
#                            on editing.  Default: Not set
#
# 0install_native_feed <src> <destpath>
#  src   - The XML file we will edit, install, and point at
#  path  - The path where the implementation will be installed
#          IE, the final edited xml will be at <path>/<basename of src>
0install_native_feed() {
	local src=$1 path=$2
	local feedfile=${src##*/}
	local dest="${path}/${feedfile}"

	0distutils "${src}" > tmp.native_feed || die "0distutils feed edit failed"

	if [[ ${ZEROINSTALL_STRIP_REQUIRES} ]]; then
		# Strip out all 'requires' sections
		sed -i -e '/<requires.*\/>/d' \
			-e '/<requires.*\>/,/<\/requires>/d' tmp.native_feed
	fi

	(
		insinto ${path}
		newins tmp.native_feed ${feedfile}
	)

	local feedname
	feedname=$(0distutils -e "${src}") || die "0distutils URI escape failed"
	dosym "${dest}" "${NATIVE_FEED_DIR}/${feedname}"

	local cachedname
	cachedname=$(0distutils -c "${src}") || die "0distutils URI escape failed"
	dosym "${path}/.DirIcon" "${ICON_CACHE_DIR}/${cachedname}"
}

# Exported functions
rox-0install_src_install() {
	# First do the regular Rox install
	rox_src_install

	# Now search for the feed, and install it if found.
	local search_list="${LOCAL_FEED_SRC} ${APPNAME}/${APPNAME}.xml ${APPNAME}.xml"
	local installed=""
	for feed in ${search_list}; do
		if [[ -f "${feed}" ]]; then
			0install_native_feed "${feed}" "${APPDIR}/${APPNAME}"
			installed="true"
			break
		fi
	done

	if [[ -z ${installed} ]]; then
		ewarn "No native feed found - This application will not be found by 0launch."
	fi
}

EXPORT_FUNCTIONS src_install
