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
# in $PATCH_DIR: ls -1 | sed -e 's/^/\t${PATCH_DIR}\//g' -e 's/$/ \\/g'
UNIPATCH_LIST="
        ${PATCH_DIR}/0001-Add-secure_modules-call.patch \
        ${PATCH_DIR}/0002-PCI-Lock-down-BAR-access-when-module-security-is-ena.patch \
        ${PATCH_DIR}/0003-x86-Lock-down-IO-port-access-when-module-security-is.patch \
        ${PATCH_DIR}/0004-ACPI-Limit-access-to-custom_method.patch \
        ${PATCH_DIR}/0005-asus-wmi-Restrict-debugfs-interface-when-module-load.patch \
        ${PATCH_DIR}/0006-Restrict-dev-mem-and-dev-kmem-when-module-loading-is.patch \
        ${PATCH_DIR}/0007-acpi-Ignore-acpi_rsdp-kernel-parameter-when-module-l.patch \
        ${PATCH_DIR}/0008-kexec-Disable-at-runtime-if-the-kernel-enforces-modu.patch \
        ${PATCH_DIR}/0009-x86-Restrict-MSR-access-when-module-loading-is-restr.patch \
        ${PATCH_DIR}/0010-Add-option-to-automatically-enforce-module-signature.patch \
        ${PATCH_DIR}/0011-efi-Make-EFI_SECURE_BOOT_SIG_ENFORCE-depend-on-EFI.patch \
        ${PATCH_DIR}/0012-efi-Add-EFI_SECURE_BOOT-bit.patch \
        ${PATCH_DIR}/0013-hibernate-Disable-in-a-signed-modules-environment.patch \
        ${PATCH_DIR}/0014-Security-Provide-copy-up-security-hooks-for-unioned-.patch \
        ${PATCH_DIR}/0015-Overlayfs-Use-copy-up-security-hooks.patch \
        ${PATCH_DIR}/0016-SELinux-Stub-in-copy-up-handling.patch \
        ${PATCH_DIR}/0017-SELinux-Handle-opening-of-a-unioned-file.patch \
        ${PATCH_DIR}/0018-SELinux-Check-against-union-label-for-file-operation.patch \
        ${PATCH_DIR}/0019-net-wireless-wl18xx-Add-missing-MODULE_FIRMWARE.patch \
        ${PATCH_DIR}/0020-overlayfs-use-a-minimal-buffer-in-ovl_copy_xattr.patch \
        ${PATCH_DIR}/0021-net-switchdev-fix-return-code-of-fdb_dump-stub.patch \
"

