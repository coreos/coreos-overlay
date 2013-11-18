# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

[[ ${EAPI} != "4" ]] && die "Only EAPI=4 is supported"

inherit cros-workon toolchain-funcs

HOMEPAGE="http://www.chromium.org/"
LICENSE="GPL-2"
SLOT="0"

DEPEND="sys-apps/debianutils
"

IUSE="-source"
STRIP_MASK="/usr/lib/debug/boot/vmlinux"

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
		find -depth -print0 | cpio "${cpio_args[@]}" -F "${cpio_path}" || \
		die "cpio update failed!")
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
	update_bootengine_cpio "${bootengine_root}"

	# Build the final kernel image
	kmake bzImage
}

cros-kernel2_src_install() {
	dodir /boot
	kmake INSTALL_PATH="${D}/boot" install
	kmake INSTALL_MOD_PATH="${D}" modules_install

	local version=$(kernelversion)
	if [ ! -e "${D}/boot/vmlinuz" ]; then
		ln -sf "vmlinuz-${version}" "${D}/boot/vmlinuz" || die
	fi

	# Install uncompressed kernel for debugging purposes.
	insinto /usr/lib/debug/lib/modules/${version}/
	doins "$(cros-workon_get_build_dir)/vmlinux"

	if use source; then
		install_kernel_sources
	else
		# Remove invalid symlinks when source isn't installed
		rm -f "${D}/lib/modules/${version}/"{build,source}
	fi
}

EXPORT_FUNCTIONS src_unpack src_configure src_compile src_install
