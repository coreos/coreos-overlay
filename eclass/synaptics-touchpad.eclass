# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

EAPI="2"
inherit eutils cros-binary

# Synaptics touchpad generic eclass.
IUSE="is_touchpad ps_touchpad"

RDEPEND="x11-base/xorg-server"
DEPEND="${RDEPEND}"

# @ECLASS-VARIABLE: SYNAPTICS_TOUCHPAD_PN
# @DESCRIPTION: The packagename used as part of the binary tarball filename.
: ${SYNAPTICS_TOUCHPAD_PN:=${PN}}

export_uri() {
	local XORG_VERSION_STRING
	local XORG_VERSION
	local X_VERSION

	XORG_VERSION_STRING=$(grep "XORG_VERSION_CURRENT" "$ROOT/usr/include/xorg/xorg-server.h")
	XORG_VERSION_STRING=${XORG_VERSION_STRING/#\#define*XORG_VERSION_CURRENT}
	XORG_VERSION=$(($XORG_VERSION_STRING))

	if [ $XORG_VERSION -ge 11100000 ]; then
		X_VERSION=1.11
	elif [ $XORG_VERSION -ge 11000000 ]; then
		X_VERSION=1.10
	elif [ $XORG_VERSION -ge 10903000 ]; then
		X_VERSION=1.9
	else
		X_VERSION=1.7
	fi
	CROS_BINARY_URI="http://commondatastorage.googleapis.com/synaptics/${SYNAPTICS_TOUCHPAD_PN}-xorg-${X_VERSION}-${PV}-${PR}.tar.gz"
}

function synaptics-touchpad_src_unpack() {
	export_uri
	cros-binary_src_unpack
}

function synaptics-touchpad_src_install() {
	# Currently you must have files/* in each ebuild that inherits
	# from here. These files will go away soon after they are pushed
	# into the synaptics tarball.
	export_uri
	cros-binary_src_install
	if [ $(ls "${D}" | wc -l) -eq 1 ]; then
		local extra_dir="$(ls "${D}")"
		mv "${D}/${extra_dir}/"* "${D}/"
		rmdir "${D}/${extra_dir}/"
	fi

	exeinto /opt/Synaptics/bin
	doexe "${FILESDIR}/tpcontrol_syncontrol" || die

	# If it exists, install synlogger to log calls to the Synaptics binaries.
	# The original binaries themselves are appended with _bin, and symlinks are
	# created with their original names that point at synlogger.
	if [ -f "${FILESDIR}/synlogger" ]; then
		doexe "${FILESDIR}/synlogger" || die
		local f
		for f in syn{control,detect,reflash} ; do
			mv "${D}"/opt/Synaptics/bin/${f}{,_bin} || die
			dosym synlogger /opt/Synaptics/bin/${f} || die
		done
	fi

	# link the appropriate config files for the type of trackpad
	if use is_touchpad && use ps_touchpad; then
	   die "Specify only one type of touchpad"
	elif use is_touchpad; then
	   dosym HKLM_Kernel_IS /opt/Synaptics/HKLM_Kernel || die
	   dosym HKLM_User_IS /opt/Synaptics/HKLM_User || die
	elif use ps_touchpad; then
	   dosym HKLM_Kernel_PS /opt/Synaptics/HKLM_Kernel || die
	   dosym HKLM_User_PS /opt/Synaptics/HKLM_User || die
	else
	   die "Type of touchpad not specified"
	fi

}

