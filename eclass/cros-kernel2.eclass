# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

[[ ${EAPI} != "4" ]] && die "Only EAPI=4 is supported"

inherit binutils-funcs cros-board toolchain-funcs

HOMEPAGE="http://www.chromium.org/"
LICENSE="GPL-2"
SLOT="0"

DEPEND="sys-apps/debianutils
"

IUSE="-device_tree -kernel_sources"
STRIP_MASK="/usr/lib/debug/boot/vmlinux"

# Build out-of-tree and incremental by default, but allow an ebuild inheriting
# this eclass to explicitly build in-tree.
: ${CROS_WORKON_OUTOFTREE_BUILD:=1}
: ${CROS_WORKON_INCREMENTAL_BUILD:=1}


# If an overlay has eclass overrides, but doesn't actually override this
# eclass, we'll have ECLASSDIR pointing to the active overlay's
# eclass/ dir, but this eclass is still in the main chromiumos tree.  So
# add a check to locate the cros-kernel/ regardless of what's going on.
ECLASSDIR_LOCAL=${BASH_SOURCE[0]%/*}
defconfig_dir() {
        local d="${ECLASSDIR}/cros-kernel"
        if [[ ! -d ${d} ]] ; then
                d="${ECLASSDIR_LOCAL}/cros-kernel"
        fi
        echo "${d}"
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

get_build_cfg() {
	echo "$(cros-workon_get_build_dir)/.config"
}

get_build_arch() {
	if [ "${ARCH}" = "arm" ] ; then
		case "${CHROMEOS_KERNEL_SPLITCONFIG}" in
			*tegra*)
				echo "tegra"
				;;
			*exynos*)
				echo "exynos5"
				;;
			*)
				echo "arm"
				;;
		esac
	else
		echo $(tc-arch-kernel)
	fi
}

# @FUNCTION: cros_chkconfig_present
# @USAGE: <option to check config for>
# @DESCRIPTION:
# Returns success of the provided option is present in the build config.
cros_chkconfig_present() {
	local config=$1
	grep -q "^CONFIG_$1=[ym]$" "$(get_build_cfg)"
}

cros-kernel2_pkg_setup() {
	# This is needed for running src_test().  The kernel code will need to
	# be rebuilt with `make check`.  If incremental build were enabled,
	# `make check` would have nothing left to build.
	use test && export CROS_WORKON_INCREMENTAL_BUILD=0
	cros-workon_pkg_setup
}

# @FUNCTION: emit_its_script
# @USAGE: <output file> <boot_dir> <dtb_dir> <device trees>
# @DESCRIPTION:
# Emits the its script used to build the u-boot fitImage kernel binary
# that contains the kernel as well as device trees used when booting
# it.

emit_its_script() {
	local iter=1
	local its_out=${1}
	shift
	local boot_dir=${1}
	shift
	local dtb_dir=${1}
	shift
	cat > "${its_out}" <<-EOF || die
	/dts-v1/;

	/ {
		description = "Chrome OS kernel image with one or more FDT blobs";
		#address-cells = <1>;

		images {
			kernel@1 {
				data = /incbin/("${boot_dir}/zImage");
				type = "$(get_kernel_type)";
				arch = "arm";
				os = "linux";
				compression = "none";
				load = <$(get_load_addr)>;
				entry = <$(get_load_addr)>;
			};
	EOF

	local dtb
	for dtb in "$@" ; do
		cat >> "${its_out}" <<-EOF || die
			fdt@${iter} {
				description = "$(basename ${dtb})";
				data = /incbin/("${dtb_dir}/${dtb}");
				type = "flat_dt";
				arch = "arm";
				compression = "none";
				hash@1 {
					algo = "sha1";
				};
			};
		EOF
		((++iter))
	done

	cat <<-EOF >>"${its_script}"
		};
		configurations {
			default = "conf@1";
	EOF

	local i
	for i in $(seq 1 $((iter-1))) ; do
		cat >> "${its_out}" <<-EOF || die
			conf@${i} {
				kernel = "kernel@1";
				fdt = "fdt@${i}";
			};
		EOF
	done

	echo "	};" >> "${its_out}"
	echo "};" >> "${its_out}"
}

kmake() {
	# Allow override of kernel arch.
	local kernel_arch=${CHROMEOS_KERNEL_ARCH:-$(tc-arch-kernel)}

	local cross=${CHOST}-
	# Hack for using 64-bit kernel with 32-bit user-space
	if [[ "${ABI:-${ARCH}}" != "amd64" && "${kernel_arch}" == "x86_64" ]]; then
		cross=x86_64-cros-linux-gnu-
	else
		# TODO(raymes): Force GNU ld over gold. There are still some
		# gold issues to iron out. See: 13209.
		tc-export LD CC CXX

		set -- \
			LD="$(get_binutils_path_ld)/ld" \
			CC="${CC} -B$(get_binutils_path_ld)" \
			CXX="${CXX} -B$(get_binutils_path_ld)" \
			"$@"
	fi

	cw_emake \
		ARCH=${kernel_arch} \
		LDFLAGS="$(raw-ldflags)" \
		CROSS_COMPILE="${cross}" \
		O="$(cros-workon_get_build_dir)" \
		"$@"
}

cros-kernel2_src_prepare() {
	cros-workon_src_prepare
}

cros-kernel2_src_configure() {
	# Use a single or split kernel config as specified in the board or variant
	# make.conf overlay. Default to the arch specific split config if an
	# overlay or variant does not set either CHROMEOS_KERNEL_CONFIG or
	# CHROMEOS_KERNEL_SPLITCONFIG. CHROMEOS_KERNEL_CONFIG is set relative
	# to the root of the kernel source tree.
	local config
	local cfgarch="$(get_build_arch)"

	if [ -n "${CHROMEOS_KERNEL_CONFIG}" ]; then
		config="${S}/${CHROMEOS_KERNEL_CONFIG}"
	else
		config=${CHROMEOS_KERNEL_SPLITCONFIG:-"chromiumos-${cfgarch}"}
	fi

	elog "Using kernel config: ${config}"

	# Keep a handle on the old .config in case it hasn't changed.  This way
	# we can keep the old timestamp which will avoid regenerating stuff that
	# hasn't actually changed.
	local temp_config="${T}/old-kernel-config"
	if [[ -e $(get_build_cfg) ]] ; then
		cp -a "$(get_build_cfg)" "${temp_config}"
	else
		rm -f "${temp_config}"
	fi

	if [ -n "${CHROMEOS_KERNEL_CONFIG}" ]; then
		cp -f "${config}" "$(get_build_cfg)" || die
	else
		if [ -e chromeos/scripts/prepareconfig ] ; then
			chromeos/scripts/prepareconfig ${config} \
				"$(get_build_cfg)" || die
		else
			config="$(defconfig_dir)/${cfgarch}_defconfig"
			ewarn "Can't prepareconfig, falling back to default " \
				"${config}"
			cp "${config}" "$(get_build_cfg)" || die
		fi
	fi

	# Use default for any options not explitly set in splitconfig
	yes "" | kmake oldconfig

	# Restore the old config if it is unchanged.
	if cmp -s "$(get_build_cfg)" "${temp_config}" ; then
		touch -r "${temp_config}" "$(get_build_cfg)"
	fi
}

# @FUNCTION: get_dtb_name
# @USAGE: <dtb_dir>
# @DESCRIPTION:
# Get the name(s) of the device tree binary file(s) to include.

get_dtb_name() {
	local dtb_dir=${1}
	local board_with_variant=$(get_current_board_with_variant)

	# Do a simple mapping for device trees whose names don't match
	# the board_with_variant format; default to just the
	# board_with_variant format.
	case "${board_with_variant}" in
		(tegra2_dev-board)
			echo tegra-harmony.dtb
			;;
		(tegra2_seaboard)
			echo tegra-seaboard.dtb
			;;
		tegra*)
			echo ${board_with_variant}.dtb
			;;
		*)
			local f
			for f in ${dtb_dir}/*.dtb ; do
			    basename ${f}
			done
			;;
	esac
}

# All current tegra boards ship with an u-boot that won't allow
# use of kernel_noload. Because of this, keep using the traditional
# kernel type for those. This means kernel_type kernel and regular
# load and entry point addresses.

get_kernel_type() {
	case "$(get_current_board_with_variant)" in
		tegra*)
			echo kernel
			;;
		*)
			echo kernel_noload
			;;
	esac
}

get_load_addr() {
	case "$(get_current_board_with_variant)" in
		tegra*)
			echo 0x03000000
			;;
		*)
			echo 0
			;;
	esac
}

cros-kernel2_src_compile() {
	local build_targets=()  # use make default target
	if use arm; then
		build_targets=(
			"uImage"
			$(cros_chkconfig_present MODULES && echo "modules")
		)
	fi

	local src_dir="$(cros-workon_get_build_dir)/source"
	local kernel_arch=${CHROMEOS_KERNEL_ARCH:-$(tc-arch-kernel)}
	SMATCH_ERROR_FILE="${src_dir}/chromeos/check/smatch_errors.log"

	if use test && [[ -e "${SMATCH_ERROR_FILE}" ]]; then
		local make_check_cmd="smatch -p=kernel"
		local test_options=(
			CHECK="${make_check_cmd}"
			C=1
		)
		SMATCH_LOG_FILE="$(cros-workon_get_build_dir)/make.log"

		# The path names in the log file are build-dependent.  Strip out
		# the part of the path before "kernel/files" and retains what
		# comes after it: the file, line number, and error message.
		kmake -k ${build_targets[@]} "${test_options[@]}" |& \
			tee "${SMATCH_LOG_FILE}"
	else
		kmake -k ${build_targets[@]}
	fi

	if use device_tree; then
		kmake -k dtbs
	fi
}

cros-kernel2_src_test() {
	[[ -e ${SMATCH_ERROR_FILE} ]] || \
		die "smatch whitelist file ${SMATCH_ERROR_FILE} not found!"
	[[ -e ${SMATCH_LOG_FILE} ]] || \
		die "Log file from src_compile() ${SMATCH_LOG_FILE} not found!"

	grep -w error: "${SMATCH_LOG_FILE}" | grep -o "kernel/files/.*" \
		| sed s:"kernel/files/"::g > "${SMATCH_LOG_FILE}.errors"
	local num_errors=$(wc -l < "${SMATCH_LOG_FILE}.errors")
	local num_warnings=$(egrep -wc "warn:|warning:" "${SMATCH_LOG_FILE}")
	einfo "smatch found ${num_errors} errors and ${num_warnings} warnings."

	# Create a version of the error database that doesn't have line numbers,
	# since line numbers will shift as code is added or removed.
	local build_dir="$(cros-workon_get_build_dir)"
	local no_line_numbers_file="${build_dir}/no_line_numbers.log"
	sed -r "s/(:[0-9]+){1,2}//" "${SMATCH_ERROR_FILE}" > \
		"${no_line_numbers_file}"

	# For every smatch error that came up during the build, check if it is
	# in the error database file.
	local num_unknown_errors=0
	local line=""
	while read line; do
		local no_line_num=$(echo "${line}" | \
			sed -r "s/(:[0-9]+){1,2}//")
		if ! fgrep -q "${no_line_num}" "${no_line_numbers_file}"; then
			eerror "Non-whitelisted error found: \"${line}\""
			: $(( ++num_unknown_errors ))
		fi
	done < "${SMATCH_LOG_FILE}.errors"

	[[ ${num_unknown_errors} -eq 0 ]] || \
		die "smatch found ${num_unknown_errors} unknown errors."
}

cros-kernel2_src_install() {
	dodir /boot
	kmake INSTALL_PATH="${D}/boot" install
	if cros_chkconfig_present MODULES; then
		kmake INSTALL_MOD_PATH="${D}" modules_install
	fi
	kmake INSTALL_MOD_PATH="${D}" firmware_install

	local version=$(kernelversion)
	if use arm; then
		local boot_dir="$(cros-workon_get_build_dir)/arch/${ARCH}/boot"
		local kernel_bin="${D}/boot/vmlinuz-${version}"
		local zimage_bin="${D}/boot/zImage-${version}"
		local dtb_dir="${boot_dir}"

		# Newer kernels (after linux-next 12/3/12) put dtbs in the dts
		# dir.  Use that if we we find no dtbs directly in boot_dir.
		# Note that we try boot_dir first since the newer kernel will
		# actually rm ${boot_dir}/*.dtb so we'll have no stale files.
		if ! ls "${dtb_dir}"/*.dtb &> /dev/null; then
			dtb_dir="${boot_dir}/dts"
		fi

		if use device_tree; then
			local its_script="$(cros-workon_get_build_dir)/its_script"
			emit_its_script "${its_script}" "${boot_dir}" \
				"${dtb_dir}" $(get_dtb_name "${dtb_dir}")
			mkimage  -f "${its_script}" "${kernel_bin}" || die
		else
			cp -a "${boot_dir}/uImage" "${kernel_bin}" || die
		fi
		cp -a "${boot_dir}/zImage" "${zimage_bin}" || die

		# TODO(vbendeb): remove the below .uimg link creation code
		# after the build scripts have been modified to use the base
		# image name.
		cd $(dirname "${kernel_bin}")
		ln -sf $(basename "${kernel_bin}") vmlinux.uimg || die
		ln -sf $(basename "${zimage_bin}") zImage || die
	fi
	if [ ! -e "${D}/boot/vmlinuz" ]; then
		ln -sf "vmlinuz-${version}" "${D}/boot/vmlinuz" || die
	fi

	# Check the size of kernel image and issue warning when image size is near
	# the limit. For factory install initramfs, we don't care about kernel
	# size limit as the image is downloaded over network.
	local kernel_image_size=$(stat -c '%s' -L "${D}"/boot/vmlinuz)
	einfo "Kernel image size is ${kernel_image_size} bytes."
	if use netboot_ramfs; then
		# No need to check kernel image size.
		true
	elif [[ ${kernel_image_size} -gt $((8 * 1024 * 1024)) ]]; then
		die "Kernel image is larger than 8 MB."
	elif [[ ${kernel_image_size} -gt $((7 * 1024 * 1024)) ]]; then
		ewarn "Kernel image is larger than 7 MB. Limit is 8 MB."
	fi

	# Install uncompressed kernel for debugging purposes.
	insinto /usr/lib/debug/boot
	doins "$(cros-workon_get_build_dir)/vmlinux"

	if use kernel_sources; then
		install_kernel_sources
	fi
}

EXPORT_FUNCTIONS pkg_setup src_prepare src_configure src_compile src_test src_install
