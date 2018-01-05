# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
ETYPE="sources"

# -rc releases should be versioned L.M_rcN
# Final releases should be versioned L.M.N, even for N == 0

# Only needed for RCs
K_BASE_VER="4.14"

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

# For 4.14 >= 4.14.10
src_install() {
	kernel-2_src_install
	local r="${PR#r0}"
	local script="${ED}/usr/src/linux-${PV/_rc/-rc}-coreos${r:+-${r}}/tools/objtool/sync-check.sh"
	if [[ -e "${script}" ]]; then
		chmod +x "${script}" || die
	fi
}

# XXX: Note we must prefix the patch filenames with "z" to ensure they are
# applied _after_ a potential patch-${KV}.patch file, present when building a
# patchlevel revision.  We mustn't apply our patches first, it fails when the
# local patches overlap with the upstream patch.
UNIPATCH_LIST="
	${PATCH_DIR}/z0001-kbuild-derive-relative-path-for-KBUILD_SRC-from-CURD.patch \
	${PATCH_DIR}/z0002-Add-arm64-coreos-verity-hash.patch \
	${PATCH_DIR}/z0003-dccp-CVE-2017-8824-use-after-free-in-DCCP-code.patch \
	${PATCH_DIR}/z0004-block-factor-out-__blkdev_issue_zero_pages.patch \
	${PATCH_DIR}/z0005-block-cope-with-WRITE-ZEROES-failing-in-blkdev_issue.patch \
	${PATCH_DIR}/z0006-x86-cpu-x86-pti-Do-not-enable-PTI-on-AMD-processors.patch \
	${PATCH_DIR}/z0007-x86-pti-Make-sure-the-user-kernel-PTEs-match.patch \
	${PATCH_DIR}/z0008-x86-pti-Switch-to-kernel-CR3-at-early-in-entry_SYSCA.patch \
	${PATCH_DIR}/z0009-x86-process-Define-cpu_tss_rw-in-same-section-as-dec.patch \
	${PATCH_DIR}/z0010-x86-mm-Set-MODULES_END-to-0xffffffffff000000.patch \
	${PATCH_DIR}/z0011-x86-mm-Map-cpu_entry_area-at-the-same-place-on-4-5-l.patch \
	${PATCH_DIR}/z0012-x86-kaslr-Fix-the-vaddr_end-mess.patch \
	${PATCH_DIR}/z0013-x86-events-intel-ds-Use-the-proper-cache-flush-metho.patch \
	${PATCH_DIR}/z0014-x86-tlb-Drop-the-_GPL-from-the-cpu_tlbstate-export.patch \
"
