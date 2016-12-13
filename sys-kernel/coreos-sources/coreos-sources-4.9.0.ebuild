# Copyright 2014 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI="5"
ETYPE="sources"
inherit kernel-2
detect_version

DESCRIPTION="Full sources for the CoreOS Linux kernel"
HOMEPAGE="http://www.kernel.org"
SRC_URI="${KERNEL_URI}"

KEYWORDS="amd64 arm64"
IUSE=""

PATCH_DIR="${FILESDIR}/${KV_MAJOR}.${KV_MINOR}"

# XXX: Note we must prefix the patch filenames with "z" to ensure they are
# applied _after_ a potential patch-${KV}.patch file, present when building a
# patchlevel revision.  We mustn't apply our patches first, it fails when the
# local patches overlap with the upstream patch.

# in $PATCH_DIR: ls -1 | sed -e 's/^/\t${PATCH_DIR}\//g' -e 's/$/ \\/g'
UNIPATCH_LIST="
        ${PATCH_DIR}/z0001-Add-secure_modules-call.patch \
        ${PATCH_DIR}/z0002-PCI-Lock-down-BAR-access-when-module-security-is-ena.patch \
        ${PATCH_DIR}/z0003-x86-Lock-down-IO-port-access-when-module-security-is.patch \
        ${PATCH_DIR}/z0004-ACPI-Limit-access-to-custom_method.patch \
        ${PATCH_DIR}/z0005-asus-wmi-Restrict-debugfs-interface-when-module-load.patch \
        ${PATCH_DIR}/z0006-Restrict-dev-mem-and-dev-kmem-when-module-loading-is.patch \
        ${PATCH_DIR}/z0007-acpi-Ignore-acpi_rsdp-kernel-parameter-when-module-l.patch \
        ${PATCH_DIR}/z0008-kexec-Disable-at-runtime-if-the-kernel-enforces-modu.patch \
        ${PATCH_DIR}/z0009-x86-Restrict-MSR-access-when-module-loading-is-restr.patch \
        ${PATCH_DIR}/z0010-Add-option-to-automatically-enforce-module-signature.patch \
        ${PATCH_DIR}/z0011-efi-Make-EFI_SECURE_BOOT_SIG_ENFORCE-depend-on-EFI.patch \
        ${PATCH_DIR}/z0012-efi-Add-EFI_SECURE_BOOT-bit.patch \
        ${PATCH_DIR}/z0013-hibernate-Disable-in-a-signed-modules-environment.patch \
        ${PATCH_DIR}/z0014-kbuild-derive-relative-path-for-KBUILD_SRC-from-CURD.patch \
"
