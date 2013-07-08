# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="coreos/vboot_reference"
CROS_WORKON_REPO="git://github.com"

inherit cros-debug cros-workon cros-au

DESCRIPTION="Chrome OS verified boot tools"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="32bit_au minimal tpmtests cros_host"

RDEPEND="!minimal? ( dev-libs/libyaml )
	dev-libs/openssl
	sys-apps/util-linux"
DEPEND="app-crypt/trousers
	${RDEPEND}"

_src_compile_main() {
	mkdir "${S}"/build-main
	tc-export CC AR CXX PKG_CONFIG
	cros-debug-add-NDEBUG
	# Vboot reference knows the flags to use
	unset CFLAGS
	emake BUILD="${S}"/build-main \
	      ARCH=$(tc-arch) \
	      MINIMAL=$(usev minimal) all
	unset CC AR CXX PKG_CONFIG
}

_src_compile_au() {
	board_setup_32bit_au_env
	mkdir "${S}"/build-au
	einfo "Building 32-bit library for installer to use"
	tc-export CC AR CXX PKG_CONFIG
	emake BUILD="${S}"/build-au/ \
	      ARCH=$(tc-arch) \
	      MINIMAL=$(usev minimal) tinyhostlib
	unset CC AR CXX PKG_CONFIG
	board_teardown_32bit_au_env
}

src_compile() {
	_src_compile_main
	use 32bit_au && _src_compile_au
}

src_test() {
	emake BUILD="${S}"/build-main \
	      ARCH=$(tc-arch) \
	      MINIMAL=$(usev minimal) runtests
}

src_install() {
	einfo "Installing programs"
	if use minimal ; then
		# Installing on the target
		emake BUILD="${S}"/build-main DESTDIR="${D}" MINIMAL=1 install

		# TODO(hungte) Since we now install all keyset into
		# /usr/share/vboot/devkeys, maybe SAFT does not need to install
		# its own keys anymore.
		einfo "Installing keys for SAFT"
		local keys_to_install='recovery_kernel_data_key.vbprivk'
		keys_to_install+=' firmware.keyblock '
		keys_to_install+=' firmware_data_key.vbprivk'
		keys_to_install+=' kernel_subkey.vbpubk'
		keys_to_install+=' kernel_data_key.vbprivk'

		insinto /usr/sbin/firmware/saft
		for key in ${keys_to_install}; do
			doins "tests/devkeys/${key}"
		done
	else
		# Installing on the host
		emake BUILD="${S}"/build-main DESTDIR="${D}/usr/bin" install
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
	insinto /usr/share/vboot/devkeys
	doins tests/devkeys/*

	# Install public headers to /build/${BOARD}/usr/include/vboot
	einfo "Installing header files"
	insinto /usr/include/vboot
	doins firmware/include/* host/include/*

	einfo "Installing host library"
	dolib.a build-main/libvboot_host.a

	# Install 32-bit library needed by installer programs.
	if use 32bit_au; then
		einfo "Installing 32-bit host library"
                insopts -m0644
                insinto /usr/lib/vboot32
                doins build-au/libvboot_host.a
	fi
}
