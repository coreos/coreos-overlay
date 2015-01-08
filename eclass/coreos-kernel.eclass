# Copyright 2013-2014 CoreOS, Inc.
# Copyright 2012 The Chromium OS Authors.
# Distributed under the terms of the GNU General Public License v2

[[ ${EAPI} != "5" ]] && die "Only EAPI=5 is supported"

inherit linux-info toolchain-funcs

HOMEPAGE="http://www.kernel.org"
LICENSE="GPL-2 freedist"
SLOT="0/${PVR}"
SRC_URI=""

DEPEND="~sys-kernel/coreos-sources-${PV}
	sys-kernel/bootengine:="

# Do not analyze or strip installed files
RESTRICT="binchecks strip"

# Use source installed by coreos-sources
KERNEL_DIR="${SYSROOT}/usr/src/linux-${PV}"
S="${KERNEL_DIR}"
# Cache kernel build tree under /var
KBUILD_OUTPUT="${SYSROOT}/var/cache/portage/${CATEGORY}/${PN}"

# Search for an apropriate defconfig in ${FILESDIR}. The config should reflect
# the kernel version but partial matching is allowed if the config is
# applicalbe to multiple ebuilds, such as different -r revisions or stable
# kernel releases. For an amd64 ebuild with version 3.12.4-r2 the order is:
# (uses the portage $ARCH instead of the kernel's for simplicity sake)
#  - amd64_defconfig-3.12.4-r2
#  - amd64_defconfig-3.12.4
#  - amd64_defconfig-3.12
#  - amd64_defconfig
# The first matching config is used, die otherwise.
find_defconfig() {
	local base_path="${FILESDIR}/${ARCH}_defconfig"
	local try_suffix try_path
	for try_suffix in "-${PVR}" "-${PV}" "-${PV%.*}" ""; do
		try_path="${base_path}${try_suffix}"
		if [[ -f "${try_path}" ]]; then
			echo "${try_path}"
			return
		fi
	done

	die "No defconfig found for ${ARCH} and ${PVR} in ${FILESDIR}"
}

# @FUNCTION: get_bootengine_lib
# @DESCRIPTION:
# Check if /lib is a symlink in the current cpio. If so we need to use
# the target path (usually lib64) instead when adding new things.
# When extracting with GNU cpio the first entry (the symlink) wins but
# in the kernel the second entry (as a directory) definition wins.
# As if using cpio isn't bad enough already.
# If lib doesn't exist or isn't a symlink then nothing is returned.
get_bootengine_lib() {
	local cpio_path="${KBUILD_OUTPUT}/bootengine.cpio"
	cpio -itv --quiet < "${cpio_path}" | \
		awk '$1 ~ /^l/ && $9 == "lib" { print $11 }'
	assert
}

# @FUNCTION: update_bootengine_cpio
# @DESCRIPTION:
# Append files in the given directory to the bootengine cpio.
# Allows us to stick kernel modules into the initramfs built into bzImage.
update_bootengine_cpio() {
	local extra_root="$1"
	local cpio_path="${KBUILD_OUTPUT}/bootengine.cpio"
	local cpio_args=(--create --append --null
		# dracut uses the 'newc' cpio format
		--format=newc
		# squash file ownership to root for new files.
		--owner=root:root
	)

	echo "Updating bootengine.cpio"
	(cd "${extra_root}" && \
		find . -print0 | cpio "${cpio_args[@]}" -F "${cpio_path}") || \
		die "cpio update failed!"
}

kmake() {
	local kernel_arch=$(tc-arch-kernel)
	# Disable hardened PIE explicitly, >=ccache-3.2 breaks the hardened
	# compiler's auto-detection of kernel builds.
	if gcc-specs-pie; then
		set -- KCFLAGS=-nopie "$@"
	fi
	emake ARCH="${kernel_arch}" CROSS_COMPILE="${CHOST}-" "$@"
}

# Discard the module signing key, we use new keys for each build.
shred_keys() {
	if [[ -e "${KBUILD_OUTPUT}"/signing_key.priv ]]; then
		shred -u "${KBUILD_OUTPUT}"/signing_key.* || die
		rm -f "${KBUILD_OUTPUT}"/x509.genkey || die
	fi
}

coreos-kernel_pkg_setup() {
	export KBUILD_OUTPUT
	addwrite "${KBUILD_OUTPUT}"
	mkdir -p -m 755 "${KBUILD_OUTPUT}" || die
	chown ${PORTAGE_USERNAME:-portage}:${PORTAGE_GRPNAME:-portage} \
		"${KBUILD_OUTPUT}" "${KBUILD_OUTPUT%/*}" || die

	# Let linux-info detect the kernel version,
	# otherwise tc-arch-kernel outputs lots of warnings.
	get_version
}

coreos-kernel_src_prepare() {
	if [[ -f ".config" || -d "include/config" ]]
	then
		die "Source is not clean! Run make mrproper in ${S}"
	fi

	local config="$(find_defconfig)"
	elog "Using kernel config: ${config}"
	cp -f "${config}" "${KBUILD_OUTPUT}/.config" || die

	# copy the cpio initrd to the output build directory so we can tack it
	# onto the kernel image itself.
	cp "${ROOT}"/usr/share/bootengine/bootengine.cpio "${KBUILD_OUTPUT}" || die

	# make sure no keys are cached from a previous build
	shred_keys
}

coreos-kernel_src_configure() {
	# Use default for any options not explitly set in defconfig
	yes "" | kmake oldconfig
}

coreos-kernel_src_compile() {
	# Build both vmlinux and modules (moddep checks symbols in vmlinux)
	kmake vmlinux modules

	# Install modules and add them to the initramfs image
	local bootengine_root="${T}/bootengine"
	kmake INSTALL_MOD_PATH="${bootengine_root}" \
		  INSTALL_MOD_STRIP="--strip-unneeded" \
		  modules_install

	local bootengine_lib=$(get_bootengine_lib)
	if [[ -n "${bootengine_lib}" ]]; then
		mkdir -p "$(dirname "${bootengine_root}/${bootengine_lib}")" || die
		mv "${bootengine_root}/lib" \
			"${bootengine_root}/${bootengine_lib}" || die
	fi
	update_bootengine_cpio "${bootengine_root}"

	# Build the final kernel image
	kmake bzImage
}

coreos-kernel_src_install() {
	dodir /usr/boot
	kmake INSTALL_PATH="${D}/usr/boot" install
	# Install firmware to a temporary (bogus) location.
	# The linux-firmware package will be used instead.
	# Stripping must be done here, not portage, to preserve sigs.
	kmake INSTALL_MOD_PATH="${D}" \
		  INSTALL_MOD_STRIP="--strip-unneeded" \
		  INSTALL_FW_PATH="${T}/fw" \
		  modules_install

	local version=$(kmake -s --no-print-directory kernelrelease)
	dosym "vmlinuz-${version}" /usr/boot/vmlinuz
	dosym "config-${version}" /usr/boot/config

	shred_keys
}

coreos-kernel_pkg_postinst() {
	# linux-info always expects to be able to find the current .config
	# so copy it into the build tree if it isn't already there.
	if ! cmp --quiet "${ROOT}/usr/boot/config" "${KBUILD_OUTPUT}/.config"; then
		cp "${ROOT}/usr/boot/config" "${KBUILD_OUTPUT}/.config"
		chown ${PORTAGE_USERNAME:-portage}:${PORTAGE_GRPNAME:-portage} \
			"${KBUILD_OUTPUT}/.config"
	fi
}

EXPORT_FUNCTIONS pkg_setup src_prepare src_configure src_compile src_install pkg_postinst
