# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
ETYPE="sources"

# -rc releases should be versioned L.M_rcN
# Final releases should be versioned L.M.N, even for n == 0

# Only needed for RCs
K_BASE_VER="4.13"

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

# XXX: Note we must prefix the patch filenames with "z" to ensure they are
# applied _after_ a potential patch-${KV}.patch file, present when building a
# patchlevel revision.  We mustn't apply our patches first, it fails when the
# local patches overlap with the upstream patch.
UNIPATCH_LIST="
	${PATCH_DIR}/z0001-efi-Add-EFI_SECURE_BOOT-bit.patch \
	${PATCH_DIR}/z0002-Add-the-ability-to-lock-down-access-to-the-running-k.patch \
	${PATCH_DIR}/z0003-efi-Lock-down-the-kernel-if-booted-in-secure-boot-mo.patch \
	${PATCH_DIR}/z0004-Enforce-module-signatures-if-the-kernel-is-locked-do.patch \
	${PATCH_DIR}/z0005-Restrict-dev-mem-and-dev-kmem-when-the-kernel-is-loc.patch \
	${PATCH_DIR}/z0006-kexec-Disable-at-runtime-if-the-kernel-is-locked-dow.patch \
	${PATCH_DIR}/z0007-Copy-secure_boot-flag-in-boot-params-across-kexec-re.patch \
	${PATCH_DIR}/z0008-kexec_file-Disable-at-runtime-if-securelevel-has-bee.patch \
	${PATCH_DIR}/z0009-hibernate-Disable-when-the-kernel-is-locked-down.patch \
	${PATCH_DIR}/z0010-uswsusp-Disable-when-the-kernel-is-locked-down.patch \
	${PATCH_DIR}/z0011-PCI-Lock-down-BAR-access-when-the-kernel-is-locked-d.patch \
	${PATCH_DIR}/z0012-x86-Lock-down-IO-port-access-when-the-kernel-is-lock.patch \
	${PATCH_DIR}/z0013-x86-Restrict-MSR-access-when-the-kernel-is-locked-do.patch \
	${PATCH_DIR}/z0014-asus-wmi-Restrict-debugfs-interface-when-the-kernel-.patch \
	${PATCH_DIR}/z0015-ACPI-Limit-access-to-custom_method-when-the-kernel-i.patch \
	${PATCH_DIR}/z0016-acpi-Ignore-acpi_rsdp-kernel-param-when-the-kernel-h.patch \
	${PATCH_DIR}/z0017-acpi-Disable-ACPI-table-override-if-the-kernel-is-lo.patch \
	${PATCH_DIR}/z0018-acpi-Disable-APEI-error-injection-if-the-kernel-is-l.patch \
	${PATCH_DIR}/z0019-bpf-Restrict-kernel-image-access-functions-when-the-.patch \
	${PATCH_DIR}/z0020-scsi-Lock-down-the-eata-driver.patch \
	${PATCH_DIR}/z0021-Prohibit-PCMCIA-CIS-storage-when-the-kernel-is-locke.patch \
	${PATCH_DIR}/z0022-Lock-down-TIOCSSERIAL.patch \
	${PATCH_DIR}/z0023-kbuild-derive-relative-path-for-KBUILD_SRC-from-CURD.patch \
	${PATCH_DIR}/z0024-Add-arm64-coreos-verity-hash.patch \
	${PATCH_DIR}/z0025-mm-thp-Do-not-make-page-table-dirty-unconditionally-.patch \
	${PATCH_DIR}/z0026-scsi-mptsas-Fixup-device-hotplug-for-VMWare-ESXi.patch \
"
