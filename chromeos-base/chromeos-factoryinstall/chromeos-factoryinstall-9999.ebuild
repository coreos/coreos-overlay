# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_PROJECT="chromiumos/platform/factory_installer"

inherit cros-workon

DESCRIPTION="Chrome OS Factory Installer"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

# Factory install images operate by downloading content from a
# server.  In some cases, the downloaded content contains programs
# to be executed.  The downloaded programs may not be complete;
# they could have dependencies on shared libraries or commands
# that must be present in the factory install image.
#
# PROVIDED_DEPEND captures a minimal set of packages promised to be
# provided for use by any downloaded program.  The list must contain
# any package depended on by any downloaded program.
#
# Currently, the only downloaded program is the firmware installer;
# the dependencies below are gleaned from eclass/cros-firmware.eclass.
# Changes in that eclass must be reflected here.
PROVIDED_DEPEND="
	app-arch/gzip
	app-arch/sharutils
	app-arch/tar
	chromeos-base/vboot_reference
	sys-apps/mosys
	sys-apps/util-linux"

# COMMON_DEPEND tracks dependencies common to both DEPEND and
# RDEPEND.
#
# For chromeos-init there's a runtime dependency because the factory
# jobs depend on upstart jobs in that package.  There's a build-time
# dependency because pkg_postinst in this ebuild edits specifc jobs
# in that package.
COMMON_DEPEND="chromeos-base/chromeos-init"

DEPEND="$COMMON_DEPEND
	x86? ( sys-boot/syslinux )"

RDEPEND="$COMMON_DEPEND
	$PROVIDED_DEPEND
	x86? ( chromeos-base/chromeos-initramfs )
	chromeos-base/chromeos-installer
	chromeos-base/memento_softwareupdate
	net-misc/htpdate
	net-wireless/iw
	sys-apps/flashrom
	sys-apps/net-tools
	sys-apps/upstart
	sys-block/parted
	sys-fs/e2fsprogs"

CROS_WORKON_LOCALNAME="factory_installer"

FACTORY_SERVER="${FACTORY_SERVER:-$(hostname -f)}"

src_install() {
	insinto /etc/init
	doins factory_install.conf
	doins factory_ui.conf

	exeinto /usr/sbin
	doexe factory_install.sh
	doexe factory_reset.sh

	insinto /root
	newins $FILESDIR/dot.factory_installer .factory_installer
	newins $FILESDIR/dot.gpt_layout .gpt_layout
	# install PMBR code
	case "$(tc-arch)" in
		"x86")
		einfo "using x86 PMBR code from syslinux"
		PMBR_SOURCE="${ROOT}/usr/share/syslinux/gptmbr.bin"
		;;
		*)
		einfo "using default PMBR code"
		PMBR_SOURCE=$FILESDIR/dot.pmbr_code
		;;
	esac
	newins $PMBR_SOURCE .pmbr_code
}

pkg_postinst() {
	[[ "$(cros_target)" != "target_image" ]] && return 0

	STATEFUL="${ROOT}/usr/local"
	STATEFUL_LSB="${STATEFUL}/etc/lsb-factory"

	mkdir -p "${STATEFUL}/etc"
	sudo dd of="${STATEFUL_LSB}" <<EOF
CHROMEOS_AUSERVER=http://${FACTORY_SERVER}:8080/update
CHROMEOS_DEVSERVER=http://${FACTORY_SERVER}:8080/update
FACTORY_INSTALL=1
HTTP_SERVER_OVERRIDE=true
# Change the below value to true to enable board prompt
USER_SELECT=false
EOF

	# never execute the updater on install shim, because firmware are
	# downloaded and installed from mini-omaha server
	touch "${ROOT}"/root/.leave_firmware_alone ||
		die "Cannot disable firmware updating"

	# Upstart honors the last 'start on' clause it finds.
	# Alter ui.conf startup script, which will make sure chrome doesn't
	# run, since it tries to update on startup.
	echo 'start on never' >> "${ROOT}/etc/init/ui.conf" ||
		die "Failed to disable UI"

	# Set network to start up another way
	sed -i 's/login-prompt-visible/started boot-services/' \
		"${ROOT}/etc/init/boot-complete.conf" ||
		die "Failed to setup network"

	# No TPM locking.
	sed -i 's/start tcsd//' \
		"${ROOT}/etc/init/tpm-probe.conf" ||
		die "Failed to disable TPM locking"

	# Stop any power management and updater daemons
	for conf in power powerd update-engine; do
		echo 'start on never' >> "${ROOT}/etc/init/$conf.conf" ||
			die "Failed to disable $conf"
	done

	# The "laptop_mode" may be triggered from udev
	rm -f "${ROOT}/etc/udev/rules.d/99-laptop-mode.rules"
}
