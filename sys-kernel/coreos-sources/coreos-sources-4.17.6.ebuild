# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
ETYPE="sources"

# -rc releases should be versioned L.M_rcN
# Final releases should be versioned L.M.N, even for N == 0

# Only needed for RCs
K_BASE_VER="4.17"

inherit kernel-2
detect_version

DESCRIPTION="Full sources for the CoreOS Linux kernel"
HOMEPAGE="http://www.kernel.org"
if [[ "${PV%%_rc*}" != "${PV}" ]]; then
	SRC_URI="https://git.kernel.org/torvalds/p/v${KV%-coreos}/v${OKV} -> patch-${KV%-coreos}.patch ${KERNEL_BASE_URI}/linux-${OKV}.tar.xz"
	PATCH_DIR="${FILESDIR}/${KV_MAJOR}.${KV_PATCH}"
else
	SRC_URI="${KERNEL_URI}"
	PATCH_DIR="${FILESDIR}/${KV_MAJOR}.${KV_MINOR}"
fi

KEYWORDS="amd64 arm64"
IUSE=""
RDEPEND+="
	sys-devel/bison
	sys-devel/flex
"

# XXX: Note we must prefix the patch filenames with "z" to ensure they are
# applied _after_ a potential patch-${KV}.patch file, present when building a
# patchlevel revision.  We mustn't apply our patches first, it fails when the
# local patches overlap with the upstream patch.
UNIPATCH_LIST="
	${PATCH_DIR}/z0001-kbuild-derive-relative-path-for-KBUILD_SRC-from-CURD.patch \
	${PATCH_DIR}/z0002-Add-arm64-coreos-verity-hash.patch \
	${PATCH_DIR}/z0003-tools-objtool-Makefile-Don-t-fail-on-fallthrough-wit.patch \
	${PATCH_DIR}/z0004-xen-netfront-Fix-mismatched-rtnl_unlock.patch \
	${PATCH_DIR}/z0005-xen-netfront-Update-features-after-registering-netde.patch \
	${PATCH_DIR}/z0006-4.17.x-won-t-boot-due-to-x86-boot-compressed-64-Hand.patch \
"
