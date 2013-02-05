# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

inherit linux-info

# @FUNCTION: create_temp_build_dir
# @DESCRIPTION:
# Creates a local copy of the kernel build tree. This allows the package to
# rebuild host-specific parts of the tree without violating the sandbox.
create_temp_build_dir() {
	get_version || die "Failed to find kernel tree"

	local dst_build=${1:-${T}/kernel-build}
	cp -pPR "$(readlink -e "${KV_OUT_DIR}")" "${dst_build}" ||
		die "Failed to copy kernel tree"

	echo "${dst_build}"
}
