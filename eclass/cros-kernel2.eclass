# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

[[ ${EAPI} != "5" ]] && die "Only EAPI=5 is supported"

inherit cros-workon toolchain-funcs

HOMEPAGE="http://www.chromium.org/"
LICENSE="GPL-2"
SLOT="0/${PVR}"

DEPEND="sys-apps/debianutils
		sys-devel/bc
		sys-kernel/bootengine:=
"

IUSE="-source symlink-usr"
RESTRICT="binchecks strip"
STRIP_MASK="/usr/lib/debug/lib/modules/*/vmlinux"

# Build out-of-tree and incremental by default, but allow an ebuild inheriting
# this eclass to explicitly build in-tree.
: ${CROS_WORKON_OUTOFTREE_BUILD:=1}
: ${CROS_WORKON_INCREMENTAL_BUILD:=1}


# Search for an apropriate defconfig in ${FILESDIR}. The config should reflect
# the kernel version but partial matching is allowed if the config is
# applicalbe to multiple ebuilds, such as different -r revisions or stable
# kernel releases. For an amd64 ebuild with version 3.12.4-r2 the order is:
#  - x86_64_defconfig-3.12.4-r2
#  - x86_64_defconfig-3.12.4
#  - x86_64_defconfig-3.12
#  - x86_64_defconfig
# The first matching config is used, die otherwise.
find_defconfig() {
	local base_path="${FILESDIR}/$(tc-arch-kernel)_defconfig"
	local try_suffix try_path
	for try_suffix in "-${PVR}" "-${PV}" "-${PV%.*}" ""; do
		try_path="${base_path}${try_suffix}"
		if [[ -f "${try_path}" ]]; then
			echo "${try_path}"
			return
		fi
	done

	die "No defconfig found for $(tc-arch-kernel) and ${PVR} in ${FILESDIR}"
}

# @FUNCTION: kernelversion
# @DESCRIPTION:
# Returns the current compiled kernel version.
# Note: Only valid after src_configure has finished running.
kernelversion() {
	kmake -s --no-print-directory kernelrelease
}

# @FUNCTION: install_kernel_sources
# @DESCRIPTION:
# Installs the kernel sources into ${D}/usr/src/${P} and fixes symlinks.
# The package must have already installed a directory under ${D}/lib/modules.
install_kernel_sources() {
	local version=$(kernelversion)
	local dest_modules_dir=lib/modules/${version}
	local dest_source_dir=usr/src/${P}
	local dest_build_dir=${dest_source_dir}/build

	# Fix symlinks in lib/modules
	ln -sfvT "../../../${dest_build_dir}" \
	   "${D}/${dest_modules_dir}/build" || die
	ln -sfvT "../../../${dest_source_dir}" \
	   "${D}/${dest_modules_dir}/source" || die

	einfo "Installing kernel source tree"
	dodir "${dest_source_dir}"
	local f
	for f in "${S}"/*; do
		[[ "$f" == "${S}/build" ]] && continue
		cp -pPR "${f}" "${D}/${dest_source_dir}" ||
			die "Failed to copy kernel source tree"
	done

	dosym "${P}" "/usr/src/linux"

	einfo "Installing kernel build tree"
	dodir "${dest_build_dir}"
	cp -pPR "$(cros-workon_get_build_dir)"/{.config,.version,Makefile,Module.symvers,include} \
		"${D}/${dest_build_dir}" || die

	# Modify Makefile to use the ROOT environment variable if defined.
	# This path needs to be absolute so that the build directory will
	# still work if copied elsewhere.
	sed -i -e "s@${S}@\$(ROOT)/${dest_source_dir}@" \
		"${D}/${dest_build_dir}/Makefile" || die
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
	local cpio_path="$(cros-workon_get_build_dir)/bootengine.cpio"
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
	local cpio_path="$(cros-workon_get_build_dir)/bootengine.cpio"
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

	local cross=${CHOST}-
	# Hack for using 64-bit kernel with 32-bit user-space
	if [[ "${ABI:-${ARCH}}" != "amd64" && "${kernel_arch}" == "x86_64" ]]; then
		cross=x86_64-cros-linux-gnu-
	fi

	cw_emake \
		ARCH=${kernel_arch} \
		LDFLAGS="$(raw-ldflags)" \
		CROSS_COMPILE="${cross}" \
		O="$(cros-workon_get_build_dir)" \
		"$@"
}

# Discard the module signing key, we use new keys for each build.
shred_keys() {
	local build_dir="$(cros-workon_get_build_dir)"
	if [[ -e "${build_dir}"/signing_key.priv ]]; then
		shred -u "${build_dir}"/signing_key.* || die
		rm -f "${build_dir}"/x509.genkey || die
	fi
}

cros-kernel2_src_unpack() {
	local srclocal="${CROS_WORKON_LOCALDIR[0]}/${CROS_WORKON_LOCALNAME[0]}"
	local srcpath="${CROS_WORKON_SRCROOT}/${srclocal}"
	if [[ -f "${srcpath}/.config" || -d "${srcpath}/include/config" ]]; then
		ewarn "Local kernel source is not clean, disabling OUTOFTREE_BUILD"
		elog "Please run 'make mrproper' in ${srclocal}"
		CROS_WORKON_OUTOFTREE_BUILD=0
	fi

	cros-workon_src_unpack

	local config="$(find_defconfig)"
	elog "Using kernel config: ${config}"
	cp -f "${config}" "$(cros-workon_get_build_dir)/.config" || die

	# copy the cpio initrd to the output build directory so we can tack it
	# onto the kernel image itself.
	cp "${ROOT}"/usr/share/bootengine/bootengine.cpio \
		"$(cros-workon_get_build_dir)" || die "copy of dracut cpio failed."

	# make sure no keys are cached from a previous build
	shred_keys
}

cros-kernel2_src_configure() {
	# Use default for any options not explitly set in defconfig
	yes "" | kmake oldconfig
}

cros-kernel2_src_compile() {
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

cros-kernel2_src_install() {
	dodir /usr/boot
	kmake INSTALL_PATH="${D}/usr/boot" install
	# Install firmware to a temporary (bogus) location.
	# The linux-firmware package will be used instead.
	# Stripping must be done here, not portage, to preserve sigs.
	kmake INSTALL_MOD_PATH="${D}" \
		  INSTALL_MOD_STRIP="--strip-unneeded" \
		  INSTALL_FW_PATH="${T}/fw" \
		  modules_install

	local version=$(kernelversion)
	dosym "vmlinuz-${version}" /usr/boot/vmlinuz

	if ! use symlink-usr; then
		dosym ../usr/boot/vmlinuz /boot/vmlinuz
	fi

	# Install uncompressed kernel for debugging purposes.
	# XXX: we haven't been using this, also we are not keeping module symbols
	# right now. Revisit both of these if we need to beef up debugging tools.
	#insinto /usr/lib/debug/lib/modules/${version}/
	#doins "$(cros-workon_get_build_dir)/vmlinux"

	if use source; then
		install_kernel_sources
	else
		# Remove invalid symlinks when source isn't installed
		rm -f "${D}/lib/modules/${version}/"{build,source}
	fi

	shred_keys
}

EXPORT_FUNCTIONS src_unpack src_configure src_compile src_install
