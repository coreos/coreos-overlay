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

UNIPATCH_LIST="${FILESDIR}/01-Add-secure_modules-call.patch \
${FILESDIR}/02-PCI-Lock-down-BAR-access-when-module-security-is-ena.patch \
${FILESDIR}/03-x86-Lock-down-IO-port-access-when-module-security-is.patch \
${FILESDIR}/04-ACPI-Limit-access-to-custom_method.patch \
${FILESDIR}/05-asus-wmi-Restrict-debugfs-interface-when-module-load.patch \
${FILESDIR}/06-Restrict-dev-mem-and-dev-kmem-when-module-loading-is.patch \
${FILESDIR}/07-acpi-Ignore-acpi_rsdp-kernel-parameter-when-module-l.patch \
${FILESDIR}/08-kexec-Disable-at-runtime-if-the-kernel-enforces-modu.patch \
${FILESDIR}/09-x86-Restrict-MSR-access-when-module-loading-is-restr.patch \
${FILESDIR}/10-Add-option-to-automatically-enforce-module-signature.patch \
${FILESDIR}/12-efi-Make-EFI_SECURE_BOOT_SIG_ENFORCE-depend-on-EFI.patch \
${FILESDIR}/13-efi-Add-EFI_SECURE_BOOT-bit.patch \
${FILESDIR}/14-hibernate-Disable-in-a-signed-modules-environment.patch"
