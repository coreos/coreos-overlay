# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-fs/udev/udev-151-r4.ebuild,v 1.14 2010/10/29 06:13:22 jer Exp $

EAPI="1"

inherit autotools eutils flag-o-matic multilib toolchain-funcs linux-info

#PATCHSET=${P}-gentoo-patchset-v1

if [[ ${PV} == "9999" ]]; then
	EGIT_REPO_URI="git://git.kernel.org/pub/scm/linux/hotplug/udev.git"
	EGIT_BRANCH="master"
	inherit git autotools
else
	# please update testsys-tarball whenever udev-xxx/test/sys/ is changed
	SRC_URI="mirror://kernel/linux/utils/kernel/hotplug/${P}.tar.bz2
			 test? ( mirror://gentoo/${PN}-151-testsys.tar.bz2 )"
	[[ -n "${PATCHSET}" ]] && SRC_URI="${SRC_URI} mirror://gentoo/${PATCHSET}.tar.bz2"
fi
DESCRIPTION="Linux dynamic and persistent device naming support (aka userspace devfs)"
HOMEPAGE="http://www.kernel.org/pub/linux/utils/kernel/hotplug/udev.html"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86"
IUSE="selinux devfs-compat old-hd-rules -extras test"

COMMON_DEPEND="selinux? ( sys-libs/libselinux )
	extras? (
		sys-apps/acl
		>=sys-apps/usbutils-0.82
		virtual/libusb:0
		sys-apps/pciutils
		dev-libs/glib:2
	)
	>=sys-apps/util-linux-2.16
	>=sys-libs/glibc-2.9"

DEPEND="${COMMON_DEPEND}
	extras? (
		dev-util/gperf
		dev-util/pkgconfig
	)
	virtual/os-headers
	!<sys-kernel/linux-headers-2.6.29
	test? ( app-text/tree )"

RDEPEND="${COMMON_DEPEND}
	!sys-apps/coldplug
	!<sys-fs/lvm2-2.02.45
	!sys-fs/device-mapper
	>=sys-apps/baselayout-1.12.5"

if [[ ${PV} == "9999" ]]; then
	# for documentation processing with xsltproc
	DEPEND="${DEPEND}
		app-text/docbook-xsl-stylesheets
		app-text/docbook-xml-dtd
		dev-util/gtk-doc"
fi

# required kernel options
CONFIG_CHECK="~INOTIFY_USER ~SIGNALFD ~!SYSFS_DEPRECATED ~!SYSFS_DEPRECATED_V2
	~!IDE"

# We need the lib/rcscripts/addon support
PROVIDE="virtual/dev-manager"

udev_check_KV() {
	local ok=0
	if [[ ${KV_MAJOR} == 2 && ${KV_MINOR} == 6 ]]
	then
		if kernel_is -ge 2 6 ${KV_PATCH_reliable} ; then
			ok=2
		elif kernel_is -ge 2 6 ${KV_PATCH_min} ; then
			ok=1
		fi
	fi
	return $ok
}

pkg_setup() {
	linux-info_pkg_setup

	udev_libexec_dir="/$(get_libdir)/udev"

	# udev requires signalfd introduced in kernel 2.6.25,
	# but a glibc compiled against >=linux-headers-2.6.27 uses the
	# new signalfd syscall introduced in kernel 2.6.27 without falling back
	# to the old one. So we just depend on 2.6.27 here, see Bug #281312.
	KV_PATCH_min=25
	KV_PATCH_reliable=27
	KV_min=2.6.${KV_PATCH_min}
	KV_reliable=2.6.${KV_PATCH_reliable}

	# always print kernel version requirements
	ewarn
	ewarn "${P} does not support Linux kernel before version ${KV_min}!"
	if [[ ${KV_PATCH_min} != ${KV_PATCH_reliable} ]]; then
		ewarn "For a reliable udev, use at least kernel ${KV_reliable}"
	fi

	echo
	# We don't care about the secondary revision of the kernel.
	# 2.6.30.4 -> 2.6.30 is all we check
	udev_check_KV
	case "$?" in
		2)	einfo "Your kernel version (${KV_FULL}) is new enough to run ${P} reliably." ;;
		1)	ewarn "Your kernel version (${KV_FULL}) is new enough to run ${P},"
			ewarn "but it may be unreliable in some cases."
			ebeep ;;
		0)	eerror "Your kernel version (${KV_FULL}) is too old to run ${P}"
			ebeep ;;
	esac
	echo

	KV_FULL_SRC=${KV_FULL}
	get_running_version
	udev_check_KV
	if [[ "$?" = "0" ]]; then
		eerror
		eerror "udev cannot be restarted after emerging,"
		eerror "as your running kernel version (${KV_FULL}) is too old."
		eerror "You really need to use a newer kernel after a reboot!"
		NO_RESTART=1
		ebeep
	fi
}

sed_libexec_dir() {
	sed -e "s#/lib/udev#${udev_libexec_dir}#" -i "$@"
}

src_unpack() {
	if [[ ${PV} == "9999" ]] ; then
		git_src_unpack
	else
		unpack ${A}

		if use test; then
			mv "${WORKDIR}"/test/sys "${S}"/test/
		fi
	fi

	cd "${S}"

	# patches go here...

	# backport some patches
	if [[ -n "${PATCHSET}" ]]; then
		EPATCH_SOURCE="${WORKDIR}/${PATCHSET}" EPATCH_SUFFIX="patch" \
	  	      EPATCH_FORCE="yes" epatch
	fi

	# Bug 301667
	epatch "${FILESDIR}"/udev-150-fix-missing-firmware-timeout.diff

	# pkgconfig fix for when ROOT != /
	epatch "${FILESDIR}"/udev-146-pkgconfig.patch

	if ! use devfs-compat; then
		# see Bug #269359
		epatch "${FILESDIR}"/udev-141-remove-devfs-names.diff
	fi

	# change rules back to group uucp instead of dialout for now
	sed -e 's/GROUP="dialout"/GROUP="uucp"/' \
		-i rules/{rules.d,packages,gentoo}/*.rules \
	|| die "failed to change group dialout to uucp"

	if [[ ${PV} != 9999 ]]; then
		# Make sure there is no sudden changes to upstream rules file
		# (more for my own needs than anything else ...)
		MD5=$(md5sum < "${S}/rules/rules.d/50-udev-default.rules")
		MD5=${MD5/  -/}
		if [[ ${MD5} != 5685cc3878df54845dda5e08d712447a ]]
		then
			echo
			eerror "50-udev-default.rules has been updated, please validate!"
			eerror "md5sum: ${MD5}"
			die "50-udev-default.rules has been updated, please validate!"
		fi
	fi

	if use old-hd-rules; then
		epatch "${FILESDIR}"/udev-151-readd-hd-rules.diff
	fi

	sed_libexec_dir \
		rules/rules.d/50-udev-default.rules \
		rules/rules.d/78-sound-card.rules \
		extras/rule_generator/write_*_rules \
		|| die "sed failed"

	if [[ ${PV} == 9999 ]]; then
		gtkdocize --copy
	fi
	epatch "${FILESDIR}"/udev-146-cross-pci-ids.patch
	epatch "${FILESDIR}"/udev-151-add-huawei-devices.patch
	epatch "${FILESDIR}"/udev-146-tmp-rules.diff
	eautoreconf
}

src_compile() {
	filter-flags -fprefetch-loop-arrays

	econf \
		--exec-prefix= \
		--libdir='${prefix}'/$(get_libdir) \
		--libexecdir='${exec_prefix}'"${udev_libexec_dir}" \
		--enable-logging \
		--with-pci-ids-path='${exec_prefix}'/usr/share/misc/pci.ids \
		$(use_with selinux) \
		$(use_enable extras) \
		--disable-introspection
	# we don't have gobject-introspection in portage tree

	emake || die "compiling udev failed"
}

src_install() {
	local scriptdir="${FILESDIR}/151-r4"

	into /
	emake DESTDIR="${D}" install || die "make install failed"

	exeinto "${udev_libexec_dir}"
	newexe "${FILESDIR}"/net-130-r1.sh net.sh	|| die "net.sh not installed properly"
	newexe "${FILESDIR}"/move_tmp_persistent_rules-112-r1.sh move_tmp_persistent_rules.sh \
		|| die "move_tmp_persistent_rules.sh not installed properly"
	newexe "${FILESDIR}"/write_root_link_rule-125 write_root_link_rule \
		|| die "write_root_link_rule not installed properly"

	doexe "${FILESDIR}"/compat_firmware.sh \
		|| die "compat_firmware.sh not installed properly"

	doexe "${scriptdir}"/shell-compat-KV.sh \
		|| die "shell-compat.sh not installed properly"
	doexe "${scriptdir}"/shell-compat-addon.sh \
		|| die "shell-compat.sh not installed properly"

	keepdir "${udev_libexec_dir}"/state
	keepdir "${udev_libexec_dir}"/devices

	# create symlinks for these utilities to /sbin
	# where multipath-tools expect them to be (Bug #168588)
	dosym "..${udev_libexec_dir}/scsi_id" /sbin/scsi_id

	# Add gentoo stuff to udev.conf
	echo "# If you need to change mount-options, do it in /etc/fstab" \
	>> "${D}"/etc/udev/udev.conf

	# let the dir exist at least
	keepdir /etc/udev/rules.d

	# Now installing rules
	cd "${S}"/rules
	insinto "${udev_libexec_dir}"/rules.d/

	# Our rules files
	doins gentoo/??-*.rules
	doins packages/40-isdn.rules

	# workaround for chromium-os:12387, must be removed for >udev-152
	doins "${FILESDIR}"/01-workaround-net-device-db.rules

	# compat-wireless firmware loading (needs compat_firmware.sh above)
	doins "${FILESDIR}"/50-compat_firmware.rules \
		|| die "compat_firmware.rules not installed properly"

	# Adding arch specific rules
	if [[ -f packages/40-${ARCH}.rules ]]
	then
		doins "packages/40-${ARCH}.rules"
	fi
	cd "${S}"

	# our udev hooks into the rc system
	insinto /$(get_libdir)/rcscripts/addons
	doins "${scriptdir}"/udev-start.sh \
		|| die "udev-start.sh not installed properly"
	doins "${scriptdir}"/udev-stop.sh \
		|| die "udev-stop.sh not installed properly"

	local init
	# udev-postmount and init-scripts for >=openrc-0.3.1, Bug #240984
	for init in udev udev-mount udev-dev-tarball udev-postmount; do
		newinitd "${scriptdir}/${init}.initd" "${init}" \
			|| die "initscript ${init} not installed properly"
	done

	# insert minimum kernel versions
	sed -e "s/%KV_MIN%/${KV_min}/" \
		-e "s/%KV_MIN_RELIABLE%/${KV_reliable}/" \
		-i "${D}"/etc/init.d/udev-mount

	# config file for init-script and start-addon
	newconfd "${scriptdir}/udev.confd" udev \
		|| die "config file not installed properly"

	insinto /etc/modprobe.d
	newins "${FILESDIR}"/blacklist-146 blacklist.conf
	newins "${FILESDIR}"/pnp-aliases pnp-aliases.conf

	# convert /lib/udev to real used dir
	sed_libexec_dir \
		"${D}/$(get_libdir)"/rcscripts/addons/*.sh \
		"${D}/${udev_libexec_dir}"/write_root_link_rule \
		"${D}"/etc/conf.d/udev \
		"${D}"/etc/init.d/udev* \
		"${D}"/etc/modprobe.d/*

	# documentation
	dodoc ChangeLog README TODO || die "failed installing docs"

	# keep doc in just one directory, Bug #281137
	rm -rf "${D}/usr/share/doc/${PN}"
	if use extras; then
		dodoc extras/keymap/README.keymap.txt || die "failed installing docs"
	fi
}

pkg_preinst() {
	# moving old files to support newer modprobe, 12 May 2009
	local f dir=${ROOT}/etc/modprobe.d/
	for f in pnp-aliases blacklist; do
		if [[ -f $dir/$f && ! -f $dir/$f.conf ]]
		then
			elog "Moving $dir/$f to $f.conf"
			mv -f "$dir/$f" "$dir/$f.conf"
		fi
	done

	if [[ -d ${ROOT}/lib/udev-state ]]
	then
		mv -f "${ROOT}"/lib/udev-state/* "${D}"/lib/udev/state/
		rm -r "${ROOT}"/lib/udev-state
	fi

	if [[ -f ${ROOT}/etc/udev/udev.config &&
	     ! -f ${ROOT}/etc/udev/udev.rules ]]
	then
		mv -f "${ROOT}"/etc/udev/udev.config "${ROOT}"/etc/udev/udev.rules
	fi

	# delete the old udev.hotplug symlink if it is present
	if [[ -h ${ROOT}/etc/hotplug.d/default/udev.hotplug ]]
	then
		rm -f "${ROOT}"/etc/hotplug.d/default/udev.hotplug
	fi

	# delete the old wait_for_sysfs.hotplug symlink if it is present
	if [[ -h ${ROOT}/etc/hotplug.d/default/05-wait_for_sysfs.hotplug ]]
	then
		rm -f "${ROOT}"/etc/hotplug.d/default/05-wait_for_sysfs.hotplug
	fi

	# delete the old wait_for_sysfs.hotplug symlink if it is present
	if [[ -h ${ROOT}/etc/hotplug.d/default/10-udev.hotplug ]]
	then
		rm -f "${ROOT}"/etc/hotplug.d/default/10-udev.hotplug
	fi

	has_version "=${CATEGORY}/${PN}-103-r3"
	previous_equal_to_103_r3=$?

	has_version "<${CATEGORY}/${PN}-104-r5"
	previous_less_than_104_r5=$?

	has_version "<${CATEGORY}/${PN}-106-r5"
	previous_less_than_106_r5=$?

	has_version "<${CATEGORY}/${PN}-113"
	previous_less_than_113=$?
}

# 19 Nov 2008
fix_old_persistent_net_rules() {
	local rules=${ROOT}/etc/udev/rules.d/70-persistent-net.rules
	[[ -f ${rules} ]] || return

	elog
	elog "Updating persistent-net rules file"

	# Change ATTRS to ATTR matches, Bug #246927
	sed -i -e 's/ATTRS{/ATTR{/g' "${rules}"

	# Add KERNEL matches if missing, Bug #246849
	sed -ri \
		-e '/KERNEL/ ! { s/NAME="(eth|wlan|ath)([0-9]+)"/KERNEL=="\1*", NAME="\1\2"/}' \
		"${rules}"
}

# See Bug #129204 for a discussion about restarting udevd
restart_udevd() {
	if [[ ${NO_RESTART} = "1" ]]; then
		ewarn "Not restarting udevd, as your kernel is too old!"
		return
	fi

	# need to merge to our system
	[[ ${ROOT} = / ]] || return

	# check if root of init-process is identical to ours (not in chroot)
	[[ -r /proc/1/root && /proc/1/root/ -ef /proc/self/root/ ]] || return

	# abort if there is no udevd running
	[[ -n $(pidof udevd) ]] || return

	# abort if no /dev/.udev exists
	[[ -e /dev/.udev ]] || return

	elog
	elog "restarting udevd now."

	killall -15 udevd &>/dev/null
	sleep 1
	killall -9 udevd &>/dev/null

	/sbin/udevd --daemon
	sleep 3
	if [[ ! -n $(pidof udevd) ]]; then
		eerror "FATAL: udev died, please check your kernel is"
		eerror "new enough and configured correctly for ${P}."
		eerror
		eerror "Please have a look at this before rebooting."
		eerror "If in doubt, please downgrade udev back to your old version"
		ebeep
	fi
}

postinst_init_scripts() {
	# FIXME: we may need some code that detects if this is a system bootstrap
	# and auto-enables udev then
	#
	# FIXME: inconsistent handling of init-scripts here
	#  * udev is added to sysinit in openrc-ebuild
	#  * we add udev-postmount to default in here
	#

	# migration to >=openrc-0.4
	if [[ -e "${ROOT}"/etc/runlevels/sysinit && ! -e "${ROOT}"/etc/runlevels/sysinit/udev ]]
	then
		ewarn
		ewarn "You need to add the udev init script to the runlevel sysinit,"
		ewarn "else your system will not be able to boot"
		ewarn "after updating to >=openrc-0.4.0"
		ewarn "Run this to enable udev for >=openrc-0.4.0:"
		ewarn "\trc-update add udev sysinit"
		ewarn
	fi

	# add udev-postmount to default runlevel instead of that ugly injecting
	# like a hotplug event, 2009/10/15

	# already enabled?
	[[ -e "${ROOT}"/etc/runlevels/default/udev-postmount ]] && return

	local enable_postmount=0
	[[ -e "${ROOT}"/etc/runlevels/sysinit/udev ]] && enable_postmount=1
	[[ "${ROOT}" = "/" && -d /dev/.udev/ ]] && enable_postmount=1

	if [[ ${enable_postmount} = 1 ]]
	then
		local initd=udev-postmount

		if [[ -e ${ROOT}/etc/init.d/${initd} ]] && \
			[[ ! -e ${ROOT}/etc/runlevels/default/${initd} ]]
		then
			ln -snf /etc/init.d/${initd} "${ROOT}"/etc/runlevels/default/${initd}
			elog "Auto-adding '${initd}' service to your default runlevel"
		fi
	else
		elog "You should add the udev-postmount service to default runlevel."
		elog "Run this to add it:"
		elog "\trc-update add udev-postmount default"
	fi
}

pkg_postinst() {
	fix_old_persistent_net_rules

	restart_udevd

	postinst_init_scripts

	# people want reminders, I'll give them reminders.  Odds are they will
	# just ignore them anyway...

	# delete 40-scsi-hotplug.rules, it is integrated in 50-udev.rules, 19 Jan 2007
	if [[ $previous_equal_to_103_r3 = 0 ]] &&
		[[ -e ${ROOT}/etc/udev/rules.d/40-scsi-hotplug.rules ]]
	then
		ewarn "Deleting stray 40-scsi-hotplug.rules"
		ewarn "installed by sys-fs/udev-103-r3"
		rm -f "${ROOT}"/etc/udev/rules.d/40-scsi-hotplug.rules
	fi

	# Removing some device-nodes we thought we need some time ago, 25 Jan 2007
	if [[ -d ${ROOT}/lib/udev/devices ]]
	then
		rm -f "${ROOT}"/lib/udev/devices/{null,zero,console,urandom}
	fi

	# Removing some old file, 29 Jan 2007
	if [[ $previous_less_than_104_r5 = 0 ]]
	then
		rm -f "${ROOT}"/etc/dev.d/net/hotplug.dev
		rmdir --ignore-fail-on-non-empty "${ROOT}"/etc/dev.d/net 2>/dev/null
	fi

	# 19 Mar 2007
	if [[ $previous_less_than_106_r5 = 0 ]] &&
		[[ -e ${ROOT}/etc/udev/rules.d/95-net.rules ]]
	then
		rm -f "${ROOT}"/etc/udev/rules.d/95-net.rules
	fi

	# Try to remove /etc/dev.d as that is obsolete, 23 Apr 2007
	if [[ -d ${ROOT}/etc/dev.d ]]
	then
		rmdir --ignore-fail-on-non-empty "${ROOT}"/etc/dev.d/default "${ROOT}"/etc/dev.d 2>/dev/null
		if [[ -d ${ROOT}/etc/dev.d ]]
		then
			ewarn "You still have the directory /etc/dev.d on your system."
			ewarn "This is no longer used by udev and can be removed."
		fi
	fi

	# 64-device-mapper.rules now gets installed by sys-fs/device-mapper
	# remove it if user don't has sys-fs/device-mapper installed, 27 Jun 2007
	if [[ $previous_less_than_113 = 0 ]] &&
		[[ -f ${ROOT}/etc/udev/rules.d/64-device-mapper.rules ]] &&
		! has_version sys-fs/device-mapper
	then
			rm -f "${ROOT}"/etc/udev/rules.d/64-device-mapper.rules
			einfo "Removed unneeded file 64-device-mapper.rules"
	fi

	# requested in bug #275974, added 2009/09/05
	ewarn
	ewarn "If after the udev update removable devices or CD/DVD drives"
	ewarn "stop working, try re-emerging HAL before filling a bug report"

	# requested in Bug #225033:
	elog
	elog "persistent-net does assigning fixed names to network devices."
	elog "If you have problems with the persistent-net rules,"
	elog "just delete the rules file"
	elog "\trm ${ROOT}etc/udev/rules.d/70-persistent-net.rules"
	elog "and then reboot."
	elog
	elog "This may however number your devices in a different way than they are now."

	ewarn
	ewarn "If you build an initramfs including udev, then please"
	ewarn "make sure that the /sbin/udevadm binary gets included,"
	ewarn "and your scripts changed to use it,as it replaces the"
	ewarn "old helper apps udevinfo, udevtrigger, ..."

	ewarn
	ewarn "mount options for directory /dev are no longer"
	ewarn "set in /etc/udev/udev.conf, but in /etc/fstab"
	ewarn "as for other directories."

	if use devfs-compat; then
		ewarn
		ewarn "devfs-compat use flag is enabled."
		ewarn "This enables devfs compatible device names."
	else
		ewarn
		ewarn "This version of udev no longer has devfs-compat enabled"
	fi
	ewarn "If you use /dev/md/*, /dev/loop/* or /dev/rd/*,"
	ewarn "then please migrate over to using the device names"
	ewarn "/dev/md*, /dev/loop* and /dev/ram*."
	ewarn "The devfs-compat rules will be removed on the next udev update."
	ewarn "For reference see Bug #269359."

	if use old-hd-rules; then
		ewarn
		ewarn "old-hd-rules use flag is enabled"
		ewarn "This adds the removed rules for /dev/hd* devices"
	else
		ewarn
		ewarn "This version of udev no longer has use flag old-hd-rules enabled"
		ewarn "So all special rules for /dev/hd* devices are missing"
	fi
	ewarn "Please migrate to the new libata if you need these rules."
	ewarn "They will be completely removed on the next udev update."

	elog
	elog "For more information on udev on Gentoo, writing udev rules, and"
	elog "         fixing known issues visit:"
	elog "         http://www.gentoo.org/doc/en/udev-guide.xml"
}
