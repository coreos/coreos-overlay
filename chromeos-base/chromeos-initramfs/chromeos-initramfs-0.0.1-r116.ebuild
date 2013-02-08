# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="bb8263a9571b78b47c7ad87e1da3a5f8a7664b0f"
CROS_WORKON_TREE="60d6e853cf014547741df550565f035321f7301f"
CROS_WORKON_PROJECT="chromiumos/platform/initramfs"

inherit cros-workon

DESCRIPTION="Create Chrome OS initramfs"
HOMEPAGE="http://www.chromium.org/"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""
DEPEND="chromeos-base/chromeos-assets
	chromeos-base/chromeos-assets-split
	chromeos-base/vboot_reference
	chromeos-base/vpd
	media-gfx/ply-image
	sys-apps/busybox[-make-symlinks]
	sys-apps/flashrom
	sys-apps/pv
	sys-fs/lvm2"
RDEPEND=""

CROS_WORKON_LOCALNAME="../platform/initramfs"

INITRAMFS_TMP_S=${WORKDIR}/initramfs_tmp
INITRAMFS_FILE="initramfs.cpio.xz"

# dobin for initramfs
idobin() {
	local src
	for src in "$@"; do
		"${FILESDIR}/copy_elf" "${ROOT}" "${INITRAMFS_TMP_S}" "${src}" ||
			die "Cannot install: $src"
		elog "Copied: $src"
	done
}

# install a list of images (presumably .png files) in /etc/screens
insimage() {
	cp "$@" "${INITRAMFS_TMP_S}"/etc/screens || die
}

build_initramfs_file() {
	local dir

	local subdirs="
		bin
		dev
		etc
		etc/screens
		lib
		log
		newroot
		proc
		stateful
		sys
		tmp
		usb
	"
	for dir in $subdirs; do
		mkdir -p "${INITRAMFS_TMP_S}/$dir" || die
	done

	# On amd64, shared libraries must live in /lib64.  More generally,
	# $(get_libdir) tells us the directory name we need for the target
	# platform's libraries.  The 'copy_elf' script installs in /lib; to
	# keep that script simple we just create a symlink to /lib, if
	# necessary.
	local libdir=$(get_libdir)
	if [ "${libdir}" != "lib" ]; then
		ln -s lib "${INITRAMFS_TMP_S}/${libdir}"
	fi

	# Copy source files not merged from our dependencies.
	cp "${S}"/init "${INITRAMFS_TMP_S}/init" || die
	chmod +x "${INITRAMFS_TMP_S}/init"
	cp "${S}"/*.sh "${INITRAMFS_TMP_S}/lib" || die

	# PNG image assets
	local shared_assets="${ROOT}"/usr/share/chromeos-assets
	insimage "${shared_assets}"/images/boot_message.png
	insimage "${S}"/assets/spinner_*.png
	insimage "${S}"/assets/icon_check.png
	insimage "${S}"/assets/icon_warning.png
	${S}/make_images "${S}/localized_text" \
					 "${INITRAMFS_TMP_S}/etc/screens" || die

	# For busybox and sh
	idobin /bin/busybox
	ln -s busybox "${INITRAMFS_TMP_S}/bin/sh"

	# For verified rootfs
	idobin /sbin/dmsetup

	# For message screen display and progress bars
	idobin /usr/bin/ply-image
	idobin /usr/bin/pv
	idobin /usr/sbin/vpd

	# /usr/sbin/vpd invokes 'flashrom' via system()
	idobin /usr/sbin/flashrom

	# For recovery behavior
	idobin /usr/bin/cgpt
	idobin /usr/bin/crossystem
	idobin /usr/bin/dump_kernel_config
	idobin /usr/bin/tpmc
	idobin /usr/bin/vbutil_kernel

	# The kernel emake expects the file in cpio format.
	( cd "${INITRAMFS_TMP_S}"
	  find . | cpio -o -H newc |
		xz -9 --check=crc32 --lzma2=dict=512KiB > \
		"${WORKDIR}/${INITRAMFS_FILE}" ) ||
		die "cannot package initramfs"
}

src_compile() {
	einfo "Creating ${INITRAMFS_FILE}"
	build_initramfs_file
	INITRAMFS_FILE_SIZE=$(stat --printf="%s" "${WORKDIR}/${INITRAMFS_FILE}")
	einfo "${INITRAMFS_FILE}: ${INITRAMFS_FILE_SIZE} bytes"
}

src_install() {
	insinto /var/lib/misc
	doins "${WORKDIR}/${INITRAMFS_FILE}"
}
