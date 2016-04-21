# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Locate all the old style config scripts this package installs.  Do it here
# here so we can search the temp $D which has only this pkg rather than the
# full ROOT which has everyone's files.
cros_pre_pkg_preinst_wrap_old_config_scripts() {
	# Only wrap when installing into a board sysroot.
	[[ $(cros_target) != "board_sysroot" ]] && return 0

	local wrappers=$(
		find "${D}"/usr/bin -name '*-config' -printf '%P ' 2>/dev/null
	)

	local wdir="${CROS_BUILD_BOARD_TREE}/bin"
	mkdir -p "${wdir}"

	local c w
	for w in ${wrappers} ; do
		# $CHOST-$CHOST-foo-config isn't helpful
		if [[ ${w} == ${CHOST}-* ]]; then
			continue
		fi
		# Skip anything that isn't a script, e.g. pkg-config
		if ! head -n1 "${D}/usr/bin/${w}" | egrep -q '^#!\s*/bin/(ba)?sh'; then
			continue
		fi
		w="${wdir}/${CHOST}-${w}"
		c="${CROS_ADDONS_TREE}/scripts/config_wrapper"
		if [[ ! -e ${w} ]] ; then
			ln -s "${c}" "${w}"
		fi
	done
}
