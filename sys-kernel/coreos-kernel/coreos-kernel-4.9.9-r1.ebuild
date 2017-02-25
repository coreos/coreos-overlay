# Copyright 2014-2016 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
COREOS_SOURCE_REVISION="-r1"
inherit coreos-kernel

DESCRIPTION="CoreOS Linux kernel"
KEYWORDS="amd64 arm64"

RDEPEND="=sys-kernel/coreos-modules-${PVR}"
DEPEND="${RDEPEND}
	app-arch/gzip
	app-shells/bash
	sys-apps/coreutils
	sys-apps/findutils
	sys-apps/grep
	sys-apps/ignition:=
	sys-apps/less
	sys-apps/sed
	sys-apps/shadow
	sys-apps/systemd
	sys-apps/seismograph
	sys-apps/util-linux
	sys-fs/btrfs-progs
	sys-fs/e2fsprogs
	sys-fs/mdadm
	sys-fs/xfsprogs
	>=sys-kernel/coreos-firmware-20160331-r1:=
	>=sys-kernel/bootengine-0.0.4:=
	sys-kernel/dracut
	virtual/udev"

# We are bad, we want to get around the sandbox.  So do the creation of the
# cpio image in pkg_setup() where we are free to mount filesystems, chroot,
# and other fun stuff.
pkg_setup() {
	coreos-kernel_pkg_setup

	[[ "${MERGE_TYPE}" == binary ]] && return

	# Fail early if we didn't detect the build installed by coreos-modules
	[[ -n "${KV_OUT_DIR}" ]] || die "Failed to detect modules build tree"

	if [[ "${ROOT:-/}" != / ]]; then
		${ROOT}/usr/sbin/update-bootengine -m -c ${ROOT} -k "${KV_FULL}" || die
	else
		update-bootengine -k "${KV_FULL}" || die
	fi
}

src_prepare() {
	# KV_OUT_DIR points to the minimal build tree installed by coreos-modules
	# Pull in the config and public module signing key
	cp -v "${KV_OUT_DIR}/.config" build/ || die
	local sig_key="$(getconfig MODULE_SIG_KEY)"
	mkdir -p "build/${sig_key%/*}" || die
	cp -v "${KV_OUT_DIR}/${sig_key}" "build/${sig_key}" || die

	# Symlink to bootengine.cpio so we can stick with relative paths in .config
	ln -sv "${ROOT}"/usr/share/bootengine/bootengine.cpio build/ || die
	config_update 'CONFIG_INITRAMFS_SOURCE="bootengine.cpio"'
}

src_compile() {
	kmake "$(kernel_target)"

	# sanity check :)
	[[ -e build/certs/signing_key.pem ]] && die "created a new key!"
}

src_install() {
	# coreos-postinst expects to find the kernel in /usr/boot
	insinto "/usr/boot"
	newins "$(kernel_path)" "vmlinuz-${KV_FULL}"
	dosym "vmlinuz-${KV_FULL}" "/usr/boot/vmlinuz"

	insinto "/usr/lib/modules/${KV_FULL}/build"
	doins build/System.map
	# TODO: vmlinux would be useful for debugging but it is >300MB

	# Uncomment vdso_install for easy access to debug symbols in gdb:
	#   set debug-file-directory /lib/modules/4.0.7-coreos-r2/vdso/
	#kmake INSTALL_MOD_PATH="${D}/usr" vdso_install
}
