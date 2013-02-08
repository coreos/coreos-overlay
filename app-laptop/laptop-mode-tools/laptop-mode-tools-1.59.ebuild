# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="2"

inherit eutils

DESCRIPTION="Linux kernel laptop_mode user-space utilities"
HOMEPAGE="http://www.samwel.tk/laptop_mode/"
SRC_URI="http://samwel.tk/laptop_mode/tools/downloads/laptop-mode-tools_1.59.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"

IUSE="-acpi -apm bluetooth -hal -scsi"

DEPEND=""

RDEPEND="sys-apps/ethtool
		acpi? ( sys-power/acpid )
		apm? ( sys-apps/apmd )
		bluetooth? (
			|| (
				net-wireless/bluez
				net-wireless/bluez-utils
			)
		)
		hal? ( sys-apps/hal )
		net-wireless/iw
		scsi? ( sys-apps/sdparm )
		sys-apps/hdparm"

PATCHES=( "0001-Enabled-laptop-mode-power-management-control-of.patch" \
          "0002-Add-config-knob-to-control-syslog-facility.patch" \
          "0003-Add-WiFi-power-management-support.patch" \
          "0005-switch-wifi-support-to-nl80211.patch" \
          "0006-Lower-hard-drive-idle-timeout-to-5-seconds.patch" \
          "0008-Export-PATH-to-which.patch" \
          "0009-only-log-VERBOSE-msgs-to-syslog-when-DEBUG.patch" \
          "0010-Do-not-run-usb-autosuspend-for-user-input-devices.patch" \
          "0012-Skip-failed-globs-when-finding-module-scripts.patch" \
          "0013-wireless-power-can-not-find-iwconfig-but-tries-to-po.patch" \
          "0014-Disable-ethernet-control.patch" \
          "0015-Disable-file-system-remount.patch" \
          "0016-Wait-for-laptop_mode-using-shell-commands.patch" \
          "0017-usb-autosuspend-black-whitelist-in-quotes.patch" \
          "0018-hdparm-check-for-valid-drive.patch" \
          "0019-board-specific-configurations.patch" \
          "0020-hdparm-skips-SSDs-for-power-management.patch" \
          "0021-alternate-config-dir.patch" \
          "0022-interactive-governor-parameters.patch" \
          "0023-disable-cpufreq-frequency-control.patch" \
          "0024-check-for-existence-of-alarm-file.patch" \
          "0025-add-blacklists-for-runtime-pm.patch" \
        )

src_unpack() {
	unpack ${A}
	for PATCH in "${PATCHES[@]}"; do
		epatch "${FILESDIR}/$PATCH"
	done
}

src_install() {
	local ignore="laptop-mode nmi-watchdog"

	dodir /etc/pm/sleep.d

	for module in ${ignore}; do
		rm usr/share/laptop-mode-tools/modules/${module}
	done

	DESTDIR="${D}" \
		MAN_D="/usr/share/man" \
		INIT_D="none" \
		APM="$(use apm && echo force || echo disabled)" \
		ACPI="$(use acpi && echo force || echo disabled)" \
		PMU="$(use pmu && echo force || echo disabled)" \
		./install.sh || die "Install failed."

	dodoc Documentation/laptop-mode.txt README
	newinitd "${FILESDIR}"/laptop_mode.init-1.4 laptop_mode

	exeinto /etc/pm/power.d
	newexe "${FILESDIR}"/laptop_mode_tools.pmutils laptop_mode_tools

	insinto /lib/udev/rules.d
	doins etc/rules/99-laptop-mode.rules
}

pkg_postinst() {
	if ! use acpi && ! use apm; then
		ewarn
		ewarn "Without USE=\"acpi\" or USE=\"apm\" ${PN} can not"
		ewarn "automatically disable laptop_mode on low battery."
		ewarn
		ewarn "This means you can lose up to 10 minutes of work if running"
		ewarn "out of battery while laptop_mode is enabled."
		ewarn
		ewarn "Please see /usr/share/doc/${PF}/laptop-mode.txt.gz for further"
		ewarn "information."
		ewarn
	fi
}
