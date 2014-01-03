# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-kernel/linux-firmware/linux-firmware-99999999.ebuild,v 1.30 2013/09/05 05:46:37 vapier Exp $

EAPI=5

if [[ ${PV} == 99999999* ]]; then
	inherit git-2
	SRC_URI=""
	EGIT_REPO_URI="git://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git"
	KEYWORDS=""
else
	SRC_URI="mirror://gentoo/linux-firmware-${PV}.tar.xz"
	KEYWORDS="amd64"
fi

DESCRIPTION="Linux firmware files"
HOMEPAGE="http://git.kernel.org/?p=linux/kernel/git/firmware/linux-firmware.git"

LICENSE="GPL-1 GPL-2 GPL-3 BSD freedist"
SLOT="0"
IUSE=""

DEPEND="sys-kernel/coreos-kernel:="
RDEPEND="${DEPEND}
	!=sys-kernel/coreos-kernel-3.12.6
	!<=sys-kernel/coreos-kernel-3.11.7-r5
	!sys-kernel/linux-firmware
	!sys-firmware/alsa-firmware[alsa_cards_ca0132]
	!sys-firmware/alsa-firmware[alsa_cards_korg1212]
	!sys-firmware/alsa-firmware[alsa_cards_maestro3]
	!sys-firmware/alsa-firmware[alsa_cards_sb16]
	!sys-firmware/alsa-firmware[alsa_cards_ymfpci]
	!media-tv/cx18-firmware
	!<sys-firmware/ivtv-firmware-20080701-r1
	!media-tv/linuxtv-dvb-firmware[dvb_cards_cx231xx]
	!media-tv/linuxtv-dvb-firmware[dvb_cards_cx23885]
	!media-tv/linuxtv-dvb-firmware[dvb_cards_usb-dib0700]
	!net-dialup/ueagle-atm
	!net-dialup/ueagle4-atm
	!net-wireless/ar9271-firmware
	!net-wireless/i2400m-fw
	!net-wireless/libertas-firmware
	!sys-firmware/rt61-firmware
	!net-wireless/rt73-firmware
	!net-wireless/rt2860-firmware
	!net-wireless/rt2870-firmware
	!sys-block/qla-fc-firmware
	!sys-firmware/amd-ucode
	!sys-firmware/iwl1000-ucode
	!sys-firmware/iwl2000-ucode
	!sys-firmware/iwl2030-ucode
	!sys-firmware/iwl3945-ucode
	!sys-firmware/iwl4965-ucode
	!sys-firmware/iwl5000-ucode
	!sys-firmware/iwl5150-ucode
	!sys-firmware/iwl6000-ucode
	!sys-firmware/iwl6005-ucode
	!sys-firmware/iwl6030-ucode
	!sys-firmware/iwl6050-ucode
	!x11-drivers/radeon-ucode
	"
#add anything else that collides to this

# source name is linux-firmware, not coreos-firmware
S="${WORKDIR}/linux-firmware-${PV}"

src_unpack() {
	if [[ ${PV} == 99999999* ]]; then
		git-2_src_unpack
	else
		default
		# rename directory from git snapshot tarball
		mv linux-firmware-*/ linux-firmware-${PV} || die
	fi
}

src_prepare() {
	# FIXME(marineam): The linux-info eclass would normally be able to
	# provide version info but since we don't install kernel source code the
	# way it expects it doesn't work correctly. Will fix this eventually...
	local kernel_pkg=$(best_version sys-kernel/coreos-kernel)
	local kernel_ver="${kernel_pkg#sys-kernel/coreos-kernel-}"
	kernel_ver="${kernel_ver%_*}"
	kernel_ver="${kernel_ver%-*}"
	local kernel_mods="${ROOT}/lib/modules/${kernel_ver}"
	# the actually version may have a + or other extraversion suffix
	local kernel_mods=$(ls -d1 "${kernel_mods}"* | tail -n1)

	# If any firmware is missing warn but don't raise a fuss. Missing
	# files either means linux-firmware probably out-of-date but since
	# this is new and hacky I'm not going to worry too much just yet.

	einfo "Scanning for files required by ${kernel_pkg}"
	echo -n > "${T}/firmware-scan"
	local kofile fwfile
	for kofile in $(find "${kernel_mods}" -name '*.ko'); do
		for fwfile in $(modinfo --field firmware "${kofile}"); do
			if [[ ! -e "${fwfile}" ]]; then
				ewarn "Missing firmware: ${fwfile} (${kofile##*/})"
			elif [[ -L "${fwfile}" ]]; then
				echo "${fwfile}" >> "${T}/firmware-scan"
				realpath --relative-to=. "${fwfile}" >> "${T}/firmware-scan"
			else
				echo "${fwfile}" >> "${T}/firmware-scan"
			fi
		done
	done

	einfo "Pruning all unneeded firmware files..."
	sort -u "${T}/firmware-scan" > "${T}/firmware"
	find * -not -type d \
		| sort "${T}/firmware" "${T}/firmware" - \
		| uniq -u | xargs -r rm
	assert

	# Prune empty directories
	find -type d -empty -delete || die
}

src_install() {
	insinto /lib/firmware/
	doins -r *
}
