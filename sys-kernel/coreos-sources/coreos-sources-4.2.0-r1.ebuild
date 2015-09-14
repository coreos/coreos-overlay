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
UNIPATCH_LIST="${PATCH_DIR}/01-Add-secure_modules-call.patch \
${PATCH_DIR}/02-PCI-Lock-down-BAR-access-when-module-security-is-ena.patch \
${PATCH_DIR}/03-x86-Lock-down-IO-port-access-when-module-security-is.patch \
${PATCH_DIR}/04-ACPI-Limit-access-to-custom_method.patch \
${PATCH_DIR}/05-asus-wmi-Restrict-debugfs-interface-when-module-load.patch \
${PATCH_DIR}/06-Restrict-dev-mem-and-dev-kmem-when-module-loading-is.patch \
${PATCH_DIR}/07-acpi-Ignore-acpi_rsdp-kernel-parameter-when-module-l.patch \
${PATCH_DIR}/08-kexec-Disable-at-runtime-if-the-kernel-enforces-modu.patch \
${PATCH_DIR}/09-x86-Restrict-MSR-access-when-module-loading-is-restr.patch \
${PATCH_DIR}/10-Add-option-to-automatically-enforce-module-signature.patch \
${PATCH_DIR}/12-efi-Make-EFI_SECURE_BOOT_SIG_ENFORCE-depend-on-EFI.patch \
${PATCH_DIR}/13-efi-Add-EFI_SECURE_BOOT-bit.patch \
${PATCH_DIR}/14-hibernate-Disable-in-a-signed-modules-environment.patch \
${PATCH_DIR}/overlayfs/0001-Security-Provide-copy-up-security-hooks-for-unioned-.patch \
${PATCH_DIR}/overlayfs/0002-Overlayfs-Use-copy-up-security-hooks.patch \
${PATCH_DIR}/overlayfs/0003-SELinux-Stub-in-copy-up-handling.patch \
${PATCH_DIR}/overlayfs/0004-SELinux-Handle-opening-of-a-unioned-file.patch \
${PATCH_DIR}/overlayfs/0005-SELinux-Check-against-union-label-for-file-operation.patch \
${PATCH_DIR}/net-wireless-wl18xx-Add-missing-MODULE_FIRMWARE.patch"
