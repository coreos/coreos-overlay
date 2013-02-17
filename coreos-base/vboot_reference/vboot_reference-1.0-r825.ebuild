# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="3e9cf90442632bed695ac0552a76ca0d1154f799"
CROS_WORKON_TREE="8b768b506c41afc82139df72f689917b51d7cbb2"
CROS_WORKON_PROJECT="chromiumos/platform/vboot_reference"

inherit cros-debug cros-workon cros-au

DESCRIPTION="Chrome OS verified boot tools"

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="32bit_au minimal rbtest tpmtests cros_host"

LIBCHROME_VERS="125070"

RDEPEND="app-crypt/trousers
	coreos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	!minimal? ( dev-libs/libyaml )
	dev-libs/glib
	dev-libs/openssl
	sys-apps/util-linux"
DEPEND="${RDEPEND}
	dev-cpp/gflags
	dev-cpp/gtest"

# We need the config in place before we run, but don't need to rebuild this
# package every time.
RDEPEND="${RDEPEND}
	!cros_host? ( coreos-base/vboot_reference-config )"

_src_compile_main() {
	mkdir "${S}"/build-main
	tc-export CC AR CXX PKG_CONFIG
	cros-debug-add-NDEBUG
	export BASE_VER=${LIBCHROME_VERS}
	# Vboot reference knows the flags to use
	unset CFLAGS
	emake BUILD="${S}"/build-main \
	      ARCH=$(tc-arch) \
	      MINIMAL=$(usev minimal) all
	unset CC AR CXX
}

_src_compile_au() {
	mkdir "${S}"/build-au
	if use 32bit_au ; then
		AU_TARGETS="libcgpt_cc libdump_kernel_config"
		einfo "Building 32-bit AU_TARGETS: ${AU_TARGETS}"
		board_setup_32bit_au_env
	else
		AU_TARGETS="libcgpt_cc libdump_kernel_config cgptmanager_tests"
		einfo "Building native AU_TARGETS: ${AU_TARGETS}"
	fi
	tc-export CC AR CXX PKG_CONFIG
	emake BUILD="${S}"/build-au/ \
	      CC="${CC}" \
	      CXX="${CXX}" \
	      ARCH=$(tc-arch) MINIMAL=$(usev minimal) \
	      ${AU_TARGETS}
	use 32bit_au && board_teardown_32bit_au_env
}

src_compile() {
	_src_compile_main
	_src_compile_au
}

src_test() {
	emake BUILD="${S}"/build-main \
	      ARCH=$(tc-arch) \
	      MINIMAL=$(usev minimal) runtests
}

src_install() {
	local dst_dir

	if use minimal ; then
		# Installing on the target. Cherry pick programs generated
		# by src_compile in the source tree build-main/ subdirectory
		einfo "Installing target programs"
		local progs='utility/dump_kernel_config'
		progs+=' utility/crossystem'
		progs+=' utility/dev_sign_file'
		progs+=' utility/dumpRSAPublicKey'
		progs+=' utility/tpm_init_temp_fix'
		progs+=' utility/tpmc'
		progs+=' utility/vbutil_key'
		progs+=' utility/vbutil_keyblock'
		progs+=' utility/vbutil_kernel'
		progs+=' utility/vbutil_firmware'
		progs+=' utility/vbutil_what_keys'
		progs+=' utility/gbb_utility'
		progs+=' utility/dump_fmap'
		progs+=' utility/dev_debug_vboot'
		progs+=' utility/enable_dev_usb_boot'
		progs+=' cgpt/cgpt'

		into /usr
		for prog in ${progs}; do
			dobin build-main/"${prog}"
		done

		einfo "Installing TPM tools"
		exeinto /usr/sbin
		doexe "utility/tpm-nvsize"
		doexe "utility/chromeos-tpm-recovery"

		einfo "Installing boot tools"
		exeinto /sbin
		doexe build-main/utility/mount-encrypted

		einfo "Installing dev tools"
		dst_dir='/usr/share/vboot/bin'
		local src_dir='scripts/image_signing'
		dodir "${dst_dir}"
		exeinto "${dst_dir}"
		doexe "${src_dir}/common_minimal.sh"
		doexe "${src_dir}/resign_firmwarefd.sh"
		doexe "${src_dir}/make_dev_firmware.sh"
		doexe "${src_dir}/make_dev_ssd.sh"
		doexe "${src_dir}/set_gbb_flags.sh"

		# TODO(hungte) Since we now install all keyset into
		# /usr/share/vboot/devkeys, maybe SAFT does not need to install
		# its own keys anymore.
		einfo "Installing keys for SAFT"
		local keys_to_install='recovery_kernel_data_key.vbprivk'
		keys_to_install+=' firmware.keyblock '
		keys_to_install+=' firmware_data_key.vbprivk'
		keys_to_install+=' kernel_subkey.vbpubk'
		keys_to_install+=' kernel_data_key.vbprivk'

		dst_dir='/usr/sbin/firmware/saft'
		dodir "${dst_dir}"
		insinto "${dst_dir}"
		for key in ${keys_to_install}; do
			doins "tests/devkeys/${key}"
		done
	else
		# Installing on host.
		emake BUILD="${S}"/build-main \
		      DESTDIR="${D}/usr/bin" install
		# EC firmware needs to compile directly from source
		dodir /usr/src/vboot
		insinto /usr/src/vboot
		doins -r firmware/include firmware/lib
	fi
	if use rbtest; then
		emake BUILD="${S}"/build-main DESTDIR="${D}/usr/bin" -C tests \
		      install-rbtest
	fi
	if use tpmtests; then
		into /usr
		# copy files starting with tpmtest, but skip .d files.
		dobin "${S}"/build-main/tests/tpm_lite/tpmtest*[^.]?
		dobin "${S}"/build-main/utility/tpm_set_readsrkpub
	fi

	# Install devkeys to /usr/share/vboot/devkeys
	# (shared by host and target)
	einfo "Installing devkeys"
	dst_dir='/usr/share/vboot/devkeys'
	dodir "${dst_dir}"
	insinto "${dst_dir}"
	doins tests/devkeys/*

	einfo "Installing header files and libraries"

	# Install firmware/include to /build/${BOARD}/usr/include/vboot
	local dst_dir='/usr/include/vboot'
	dodir "${dst_dir}"
	insinto "${dst_dir}"
	doins -r firmware/include/*
	for arch in $(ls firmware/arch/); do
		insinto "${dst_dir}"/arch/"${arch}"
		doins firmware/arch/"${arch}"/include/biosincludes.h
	done

	insinto /usr/include/vboot/
	doins "utility/include/kernel_blob.h"
	doins "utility/include/dump_kernel_config.h"
	doins "cgpt/CgptManager.h"
	doins "firmware/lib/cgptlib/include/gpt.h"

	# Install static library needed by install programs.
	# we need board_setup_32bit_au_env again so dolib.a installs to the
	# correct location
	use 32bit_au && board_setup_32bit_au_env

	einfo "Installing dump_kernel_config library"
	dolib.a build-au/libdump_kernel_config.a

	einfo "Installing C++ version of cgpt static library:libcgpt-cc.a"
	dolib.a build-au/cgpt/libcgpt-cc.a

	use 32bit_au && board_teardown_32bit_au_env
}
