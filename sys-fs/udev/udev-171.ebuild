# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-fs/udev/udev-171-r2.ebuild,v 1.2 2011/09/18 06:42:42 zmedico Exp $

EAPI=4

KV_min=2.6.32
KV_reliable=2.6.32
PATCHSET=${P}-gentoo-patchset-v1
scriptversion=v4
scriptname=udev-gentoo-scripts-${scriptversion}

if [[ ${PV} == "9999" ]]
then
	EGIT_REPO_URI="git://git.kernel.org/pub/scm/linux/hotplug/udev.git"
	EGIT_BRANCH="master"
	vcs="git-2 autotools"
fi

inherit ${vcs} eutils flag-o-matic multilib toolchain-funcs linux-info systemd libtool

if [[ ${PV} != "9999" ]]
then
	KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~m68k ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86 ~x86-linux"
	# please update testsys-tarball whenever udev-xxx/test/sys/ is changed
	SRC_URI="mirror://kernel/linux/utils/kernel/hotplug/${P}.tar.bz2
			 test? ( mirror://gentoo/${PN}-171-testsys.tar.bz2 )"
	if [[ -n "${PATCHSET}" ]]
	then
		SRC_URI="${SRC_URI} mirror://gentoo/${PATCHSET}.tar.bz2"
	fi
fi
SRC_URI="${SRC_URI} mirror://gentoo/${scriptname}.tar.bz2"

DESCRIPTION="Linux dynamic and persistent device naming support (aka userspace devfs)"
HOMEPAGE="http://www.kernel.org/pub/linux/utils/kernel/hotplug/udev.html"

LICENSE="GPL-2"
SLOT="0"
IUSE="build selinux test debug +rule_generator hwdb acl gudev introspection
	keymap floppy edd action_modeswitch extras"

COMMON_DEPEND="selinux? ( sys-libs/libselinux )
	extras? ( sys-apps/acl
		dev-libs/glib:2
		dev-libs/gobject-introspection
		virtual/libusb:0 )
	acl? ( sys-apps/acl dev-libs/glib:2 )
	gudev? ( dev-libs/glib:2 )
	introspection? ( dev-libs/gobject-introspection )
	action_modeswitch? ( virtual/libusb:0 )
	>=sys-apps/util-linux-2.16
	>=sys-libs/glibc-2.10"

DEPEND="${COMMON_DEPEND}
	keymap? ( dev-util/gperf )
	extras? ( dev-util/gperf )
	dev-util/pkgconfig
	virtual/os-headers
	!<sys-kernel/linux-headers-2.6.34
	test? ( app-text/tree )"

RDEPEND="${COMMON_DEPEND}
	hwdb?
	(
		>=sys-apps/usbutils-0.82
		sys-apps/pciutils
	)
	extras?
	(
		>=sys-apps/usbutils-0.82
		sys-apps/pciutils
	)
	!sys-apps/coldplug
	!<sys-fs/lvm2-2.02.45
	!sys-fs/device-mapper
	>=sys-apps/baselayout-1.12.5"

if [[ ${PV} == "9999" ]]
then
	# for documentation processing with xsltproc
	DEPEND="${DEPEND}
		app-text/docbook-xsl-stylesheets
		app-text/docbook-xml-dtd
		dev-util/gtk-doc"
fi

# required kernel options
CONFIG_CHECK="~INOTIFY_USER ~SIGNALFD ~!SYSFS_DEPRECATED ~!SYSFS_DEPRECATED_V2
	~!IDE ~BLK_DEV_BSG"

# Return values:
# 2 - reliable
# 1 - unreliable
# 0 - too old
udev_check_KV() {
	local ok=0
	if kernel_is -ge ${KV_reliable//./ }
	then
		ok=2
	elif kernel_is -ge ${KV_min//./ }
	then
		ok=1
	fi
	return $ok
}

pkg_setup() {
	linux-info_pkg_setup

	# always print kernel version requirements
	ewarn
	ewarn "${P} does not support Linux kernel before version ${KV_min}!"
	if [[ ${KV_min} != ${KV_reliable} ]]
	then
		ewarn "For a reliable udev, use at least kernel ${KV_reliable}"
	fi

	udev_check_KV
	case "$?" in
		2)	einfo "Your kernel version (${KV_FULL}) is new enough to run ${P} reliably." ;;
		1)	ewarn "Your kernel version (${KV_FULL}) is new enough to run ${P},"
			ewarn "but it may be unreliable in some cases."
			;;
		0)	eerror "Your kernel version (${KV_FULL}) is too old to run ${P}"
			;;
	esac

	KV_FULL_SRC=${KV_FULL}
	get_running_version
	udev_check_KV
	if [[ "$?" = "0" ]]
	then
		eerror
		eerror "udev cannot be restarted after emerging,"
		eerror "as your running kernel version (${KV_FULL}) is too old."
		eerror "You really need to use a newer kernel after a reboot!"
		NO_RESTART=1
	fi
}

src_unpack() {
	unpack ${A}
	if [[ ${PV} == "9999" ]]
	then
		git-2_src_unpack
	fi
}

src_prepare() {
	if use test && [[ -d "${WORKDIR}"/test/sys ]]
	then
		mv "${WORKDIR}"/test/sys "${S}"/test/
	fi

	# patches go here...
	epatch "${FILESDIR}"/udev-170-fusectl-opts.patch

	# backport some patches
	if [[ -n "${PATCHSET}" ]]
	then
		EPATCH_SOURCE="${WORKDIR}/${PATCHSET}" EPATCH_SUFFIX="patch" \
			  EPATCH_FORCE="yes" epatch
	fi

	# change rules back to group uucp instead of dialout for now
	sed -e 's/GROUP="dialout"/GROUP="uucp"/' \
		-i rules/{rules.d,arch}/*.rules \
	|| die "failed to change group dialout to uucp"

	if [[ ${PV} == 9999 ]]
	then
		gtkdocize --copy || die "gtkdocize failed"
		eautoreconf
	else
		elibtoolize
	fi
}

src_configure() {
	if ! use extras
	then
	econf \
		--prefix="${EPREFIX}/usr" \
		--sysconfdir="${EPREFIX}/etc" \
		--sbindir="${EPREFIX}/sbin" \
		--libdir="${EPREFIX}/usr/$(get_libdir)" \
		--with-rootlibdir="${EPREFIX}/$(get_libdir)" \
		--libexecdir="${EPREFIX}/lib/udev" \
		--enable-logging \
		--enable-static \
		$(use_with selinux) \
		$(use_enable debug) \
		$(use_enable rule_generator) \
		$(use_enable hwdb) \
		--with-pci-ids-path="${EPREFIX}/usr/share/misc/pci.ids" \
		--with-usb-ids-path="${EPREFIX}/usr/share/misc/usb.ids" \
		$(use_enable acl udev_acl) \
		$(use_enable gudev) \
		$(use_enable introspection) \
		$(use_enable keymap) \
		$(use_enable floppy) \
		$(use_enable edd) \
		$(use_enable action_modeswitch) \
		$(systemd_with_unitdir)
	else
	econf \
		--prefix="${EPREFIX}/usr" \
		--sysconfdir="${EPREFIX}/etc" \
		--sbindir="${EPREFIX}/sbin" \
		--libdir="${EPREFIX}/usr/$(get_libdir)" \
		--with-rootlibdir="${EPREFIX}/$(get_libdir)" \
		--libexecdir="${EPREFIX}/lib/udev" \
		--enable-logging \
		--enable-static \
		$(use_with selinux) \
		$(use_enable debug) \
		--enable-rule_generator \
		--enable-hwdb \
		--with-pci-ids-path="${EPREFIX}/usr/share/misc/pci.ids" \
		--with-usb-ids-path="${EPREFIX}/usr/share/misc/usb.ids" \
		--enable-udev_acl \
		--enable-gudev \
		--enable-introspection \
		--enable-keymap \
		--enable-floppy \
		--enable-edd \
		--enable-action_modeswitch \
		$(systemd_with_unitdir)
	fi
}

src_compile() {
	filter-flags -fprefetch-loop-arrays

	emake
}

src_install() {
	emake -C "${WORKDIR}/${scriptname}" \
		DESTDIR="${D}" LIBDIR="${EPREFIX}$(get_libdir)" \
		LIBUDEV="${EPREFIX}/lib/udev" \
		MODPROBE_DIR="${EPREFIX}/etc/modprobe.d" \
		INITD="${EPREFIX}/etc/init.d" \
		CONFD="${EPREFIX}/etc/conf.d" \
		KV_min="${KV_min}" KV_reliable="${KV_reliable}" \
		install

	into /
	emake DESTDIR="${D}" install

	exeinto /lib/udev
	keepdir /lib/udev/state
	keepdir /lib/udev/devices

	# create symlinks for these utilities to /sbin
	# where multipath-tools expect them to be (Bug #168588)
	dosym "../lib/udev/scsi_id" /sbin/scsi_id

	# Add gentoo stuff to udev.conf
	echo "# If you need to change mount-options, do it in /etc/fstab" \
	>> "${ED}"/etc/udev/udev.conf

	# let the dir exist at least
	keepdir /etc/udev/rules.d

	# Now installing rules
	cd "${S}"/rules
	insinto /lib/udev/rules.d/

	# support older kernels
	doins misc/30-kernel-compat.rules

	# Adding arch specific rules
	if [[ -f arch/40-${ARCH}.rules ]]
	then
		doins "arch/40-${ARCH}.rules"
	fi
	cd "${S}"

	insinto /etc/modprobe.d
	newins "${FILESDIR}"/blacklist-146 blacklist.conf
	newins "${FILESDIR}"/pnp-aliases pnp-aliases.conf

	# documentation
	dodoc ChangeLog README TODO

	# keep doc in just one directory, Bug #281137
	rm -rf "${ED}/usr/share/doc/${PN}"
	if use keymap
	then
		dodoc extras/keymap/README.keymap.txt
	fi
}

src_test() {
	local emake_cmd="${MAKE:-make} ${MAKEOPTS} ${EXTRA_EMAKE}"
	cd "${WORKDIR}/${scriptname}"
	vecho ">>> Test phase [scripts:test]: ${CATEGORY}/${PF}"
	if ! $emake_cmd -j1 test
	then
		has test $FEATURES && die "scripts: Make test failed. See above for details."
		has test $FEATURES || eerror "scripts: Make test failed. See above for details."
	fi

	cd "${S}"
	vecho ">>> Test phase [udev:check]: ${CATEGORY}/${PF}"
	has userpriv $FEATURES && einfo "Disable FEATURES userpriv to run the udev tests"
	if ! $emake_cmd -j1 check
	then
		has test $FEATURES && die "udev: Make test failed. See above for details."
		has test $FEATURES || eerror "udev: Make test failed. See above for details."
	fi
}

pkg_preinst() {
	# moving old files to support newer modprobe, 12 May 2009
	local f dir=${EROOT}/etc/modprobe.d/
	for f in pnp-aliases blacklist; do
		if [[ -f $dir/$f && ! -f $dir/$f.conf ]]
		then
			elog "Moving $dir/$f to $f.conf"
			mv -f "$dir/$f" "$dir/$f.conf"
		fi
	done

	if [[ -d ${EROOT}/lib/udev-state ]]
	then
		mv -f "${EROOT}"/lib/udev-state/* "${ED}"/lib/udev/state/
		rm -r "${EROOT}"/lib/udev-state
	fi

	if [[ -f ${EROOT}/etc/udev/udev.config &&
		 ! -f ${EROOT}/etc/udev/udev.rules ]]
	then
		mv -f "${EROOT}"/etc/udev/udev.config "${EROOT}"/etc/udev/udev.rules
	fi

	# delete the old udev.hotplug symlink if it is present
	if [[ -h ${EROOT}/etc/hotplug.d/default/udev.hotplug ]]
	then
		rm -f "${EROOT}"/etc/hotplug.d/default/udev.hotplug
	fi

	# delete the old wait_for_sysfs.hotplug symlink if it is present
	if [[ -h ${EROOT}/etc/hotplug.d/default/05-wait_for_sysfs.hotplug ]]
	then
		rm -f "${EROOT}"/etc/hotplug.d/default/05-wait_for_sysfs.hotplug
	fi

	# delete the old wait_for_sysfs.hotplug symlink if it is present
	if [[ -h ${EROOT}/etc/hotplug.d/default/10-udev.hotplug ]]
	then
		rm -f "${EROOT}"/etc/hotplug.d/default/10-udev.hotplug
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
	local rules=${EROOT}/etc/udev/rules.d/70-persistent-net.rules
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
	if [[ ${NO_RESTART} = "1" ]]
	then
		ewarn "Not restarting udevd, as your kernel is too old!"
		return
	fi

	# need to merge to our system
	[[ ${EROOT} = / ]] || return

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
	if [[ ! -n $(pidof udevd) ]]
	then
		eerror "FATAL: udev died, please check your kernel is"
		eerror "new enough and configured correctly for ${P}."
		eerror
		eerror "Please have a look at this before rebooting."
		eerror "If in doubt, please downgrade udev back to your old version"
	fi
}

postinst_init_scripts() {
	local enable_postmount=false

	# FIXME: inconsistent handling of init-scripts here
	#  * udev is added to sysinit in openrc-ebuild
	#  * we add udev-postmount to default in here
	#

	# If we are building stages, add udev to the sysinit runlevel automatically.
	if use build
	then
		if [[ -x "${EROOT}"/etc/init.d/udev  \
			&& -d "${EROOT}"/etc/runlevels/sysinit ]]
		then
			ln -s "${EPREFIX}"/etc/init.d/udev "${EROOT}"/etc/runlevels/sysinit/udev
		fi
		enable_postmount=true
	fi

	# migration to >=openrc-0.4
	if [[ -e "${EROOT}"/etc/runlevels/sysinit && ! -e "${EROOT}"/etc/runlevels/sysinit/udev ]]
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
	[[ -e "${EROOT}"/etc/runlevels/default/udev-postmount ]] && return

	[[ -e "${EROOT}"/etc/runlevels/sysinit/udev ]] && enable_postmount=true
	[[ "${EROOT}" = "/" && -d /dev/.udev/ ]] && enable_postmount=true

	if $enable_postmount
	then
		local initd=udev-postmount

		if [[ -e ${EROOT}/etc/init.d/${initd} ]] && \
			[[ ! -e ${EROOT}/etc/runlevels/default/${initd} ]]
		then
			ln -snf "${EPREFIX}"/etc/init.d/${initd} "${EROOT}"/etc/runlevels/default/${initd}
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

	# "losetup -f" is confused if there is an empty /dev/loop/, Bug #338766
	# So try to remove it here (will only work if empty).
	rmdir "${EROOT}"/dev/loop 2>/dev/null
	if [[ -d "${EROOT}"/dev/loop ]]
	then
		ewarn "Please make sure your remove /dev/loop,"
		ewarn "else losetup may be confused when looking for unused devices."
	fi

	restart_udevd

	postinst_init_scripts

	# people want reminders, I'll give them reminders.  Odds are they will
	# just ignore them anyway...

	# delete 40-scsi-hotplug.rules, it is integrated in 50-udev.rules, 19 Jan 2007
	if [[ $previous_equal_to_103_r3 = 0 ]] &&
		[[ -e ${EROOT}/etc/udev/rules.d/40-scsi-hotplug.rules ]]
	then
		ewarn "Deleting stray 40-scsi-hotplug.rules"
		ewarn "installed by sys-fs/udev-103-r3"
		rm -f "${EROOT}"/etc/udev/rules.d/40-scsi-hotplug.rules
	fi

	# Removing some device-nodes we thought we need some time ago, 25 Jan 2007
	if [[ -d ${EROOT}/lib/udev/devices ]]
	then
		rm -f "${EROOT}"/lib/udev/devices/{null,zero,console,urandom}
	fi

	# Removing some old file, 29 Jan 2007
	if [[ $previous_less_than_104_r5 = 0 ]]
	then
		rm -f "${EROOT}"/etc/dev.d/net/hotplug.dev
		rmdir --ignore-fail-on-non-empty "${EROOT}"/etc/dev.d/net 2>/dev/null
	fi

	# 19 Mar 2007
	if [[ $previous_less_than_106_r5 = 0 ]] &&
		[[ -e ${EROOT}/etc/udev/rules.d/95-net.rules ]]
	then
		rm -f "${EROOT}"/etc/udev/rules.d/95-net.rules
	fi

	# Try to remove /etc/dev.d as that is obsolete, 23 Apr 2007
	if [[ -d ${EROOT}/etc/dev.d ]]
	then
		rmdir --ignore-fail-on-non-empty "${EROOT}"/etc/dev.d/default "${EROOT}"/etc/dev.d 2>/dev/null
		if [[ -d ${EROOT}/etc/dev.d ]]
		then
			ewarn "You still have the directory /etc/dev.d on your system."
			ewarn "This is no longer used by udev and can be removed."
		fi
	fi

	# 64-device-mapper.rules now gets installed by sys-fs/device-mapper
	# remove it if user don't has sys-fs/device-mapper installed, 27 Jun 2007
	if [[ $previous_less_than_113 = 0 ]] &&
		[[ -f ${EROOT}/etc/udev/rules.d/64-device-mapper.rules ]] &&
		! has_version sys-fs/device-mapper
	then
			rm -f "${EROOT}"/etc/udev/rules.d/64-device-mapper.rules
			einfo "Removed unneeded file 64-device-mapper.rules"
	fi

	# requested in Bug #225033:
	elog
	elog "persistent-net does assigning fixed names to network devices."
	elog "If you have problems with the persistent-net rules,"
	elog "just delete the rules file"
	elog "\trm ${EROOT}etc/udev/rules.d/70-persistent-net.rules"
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

	ewarn
	ewarn "If you use /dev/md/*, /dev/loop/* or /dev/rd/*,"
	ewarn "then please migrate over to using the device names"
	ewarn "/dev/md*, /dev/loop* and /dev/ram*."
	ewarn "The devfs-compat rules have been removed."
	ewarn "For reference see Bug #269359."

	ewarn
	ewarn "Rules for /dev/hd* devices have been removed"
	ewarn "Please migrate to libata."

	elog
	elog "For more information on udev on Gentoo, writing udev rules, and"
	elog "         fixing known issues visit:"
	elog "         http://www.gentoo.org/doc/en/udev-guide.xml"
}
