# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="Chrome OS Fonts (meta package)"
HOMEPAGE="http://src.chromium.org"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cros_host internal"

# Internal and external builds deliver different fonts for Japanese.
# Although the two fonts can in theory co-exist, the font selection
# code in the chromeos-initramfs build prefers one or the other, but
# not both.
#
# The build system will actually try to make both fonts co-exist in
# some cases, because the default chroot downloaded by cros_sdk
# includes the ja-ipafonts package.  The logic here also protects
# in the case that you switch a repo from internal to external, and
# vice-versa.
JA_FONTS="
	internal? (
		chromeos-base/ja-motoyafonts
		!media-fonts/ja-ipafonts
	)
	!internal? (
		!chromeos-base/ja-motoyafonts
		media-fonts/ja-ipafonts
	)
	"

# List of font packages used in Chromium OS.  This list is separate
# so that it can be shared between the host in
# chromeos-base/hard-host-depends and the target in
# chromeos-base/chromeos.
#
# The glibc requirement is a bit funky.  For target boards, we make sure it is
# installed before any other package (by way of setup_board), but for the sdk
# board, we don't have that toolchain-specific tweak.  So we end up installing
# these in parallel and the chroot logic for font generation fails.  We can
# drop this when we stop executing the helper in the $ROOT via `chroot` and/or
# `qemu` (e.g. when we do `ROOT=/build/amd64-host/ emerge chromeos-fonts`).
RDEPEND="
	${JA_FONTS}
	!cros_host? ( chromeos-base/chromeos-assets )
	media-fonts/croscorefonts
	media-fonts/notofonts
	media-fonts/dejavu
	media-fonts/droidfonts-cros
	media-fonts/ko-nanumfonts
	media-fonts/lohitfonts-cros
	media-fonts/ml-anjalioldlipi
	media-fonts/sil-abyssinica
	media-libs/fontconfig
	cros_host? ( sys-libs/glibc )
	"

qemu_run() {
	# Run the emulator to execute command. It needs to be copied
	# temporarily into the sysroot because we chroot to it.
	local qemu
	case "${ARCH}" in
		amd64)
			# Note that qemu is not actually run below in this case.
			qemu="qemu-x86_64"
			;;
		arm)
			qemu="qemu-arm"
			;;
		x86)
			qemu="qemu-i386"
			;;
		*)
			die "Unable to determine QEMU from ARCH."
	esac

	# If we're running directly on the target (e.g. gmerge), we don't need to
	# chroot or use qemu.
	if [ "${ROOT:-/}" = "/" ]; then
		"$@" || die
	elif [ "${ARCH}" = "amd64" ] || [ "${ARCH}" = "x86" ]; then
		chroot "${ROOT}" "$@" || die
	else
		cp "/usr/bin/${qemu}" "${ROOT}/tmp" || die
		chroot "${ROOT}" "/tmp/${qemu}" "$@" || die
		rm "${ROOT}/tmp/${qemu}" || die
	fi
}

generate_font_cache() {
	mkdir -p "${ROOT}/usr/share/fontconfig" || die
	# fc-cache needs the font files to be located in their final resting place.
	qemu_run /usr/bin/fc-cache -f
}

pkg_preinst() {
	generate_font_cache
}
