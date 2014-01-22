# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/systemd/systemd-204.ebuild,v 1.12 2013/09/06 19:58:12 pacho Exp $

EAPI=5

AUTOTOOLS_PRUNE_LIBTOOL_FILES=all
PYTHON_COMPAT=( python2_7 )
inherit autotools-utils bash-completion-r1 linux-info multilib pam python-single-r1 systemd toolchain-funcs udev user

DESCRIPTION="System and service manager for Linux"
HOMEPAGE="http://www.freedesktop.org/wiki/Software/systemd"
SRC_URI="http://www.freedesktop.org/software/systemd/${P}.tar.xz"

LICENSE="GPL-2 LGPL-2.1 MIT"
SLOT="0"
KEYWORDS="amd64 arm ppc ppc64 x86"
IUSE="acl audit cryptsetup doc +firmware-loader gcrypt gudev http introspection
	keymap +kmod lzma openrc pam policykit python qrcode selinux static-libs
	tcpd test vanilla xattr"

MINKV="2.6.39"

COMMON_DEPEND=">=sys-apps/dbus-1.6.8-r1
	>=sys-apps/util-linux-2.20
	sys-libs/libcap
	acl? ( sys-apps/acl )
	audit? ( >=sys-process/audit-2 )
	cryptsetup? ( >=sys-fs/cryptsetup-1.4.2 )
	gcrypt? ( >=dev-libs/libgcrypt-1.4.5 )
	gudev? ( >=dev-libs/glib-2 )
	http? ( net-libs/libmicrohttpd )
	introspection? ( >=dev-libs/gobject-introspection-1.31.1 )
	kmod? ( >=sys-apps/kmod-12 )
	lzma? ( app-arch/xz-utils )
	pam? ( virtual/pam )
	python? ( ${PYTHON_DEPS} )
	qrcode? ( media-gfx/qrencode )
	selinux? ( sys-libs/libselinux )
	tcpd? ( sys-apps/tcp-wrappers )
	xattr? ( sys-apps/attr )"

# baselayout-2.2 has /run
RDEPEND="${COMMON_DEPEND}
	>=sys-apps/baselayout-2.2
	openrc? ( >=sys-fs/udev-init-scripts-25 )
	policykit? ( sys-auth/polkit )
	|| (
		>=sys-apps/util-linux-2.22
		<sys-apps/sysvinit-2.88-r4
	)
	!sys-auth/nss-myhostname
	!<sys-libs/glibc-2.10
	!sys-fs/udev"

PDEPEND=">=sys-apps/hwids-20130326.1[udev]"

DEPEND="${COMMON_DEPEND}
	app-arch/xz-utils
	app-text/docbook-xml-dtd:4.2
	app-text/docbook-xsl-stylesheets
	dev-libs/libxslt
	dev-util/gperf
	>=dev-util/intltool-0.50
	>=sys-devel/gcc-4.6
	>=sys-kernel/linux-headers-${MINKV}
	virtual/pkgconfig
	doc? ( >=dev-util/gtk-doc-1.18 )"

pkg_pretend() {
	local CONFIG_CHECK="~AUTOFS4_FS ~BLK_DEV_BSG ~CGROUPS ~DEVTMPFS
		~FANOTIFY ~HOTPLUG ~INOTIFY_USER ~IPV6 ~NET ~PROC_FS ~SIGNALFD
		~SYSFS ~!IDE ~!SYSFS_DEPRECATED ~!SYSFS_DEPRECATED_V2"
#		~!FW_LOADER_USER_HELPER"

	use acl && CONFIG_CHECK+=" ~TMPFS_POSIX_ACL"

	# read null-terminated argv[0] from PID 1
	# and see which path to systemd was used (if any)
	local init_path
	IFS= read -r -d '' init_path < /proc/1/cmdline
	if [[ ${init_path} == */bin/systemd ]]; then
		eerror "You are using a compatibility symlink to run systemd. The symlink"
		eerror "has been removed. Please update your bootloader to use:"
		eerror
		eerror "	init=/usr/lib/systemd/systemd"
		eerror
		eerror "and reboot your system. We are sorry for the inconvenience."
		if [[ ${MERGE_TYPE} != buildonly ]]; then
			die "Compatibility symlink used to boot systemd."
		fi
	fi

	if [[ ${MERGE_TYPE} != binary ]]; then
		if [[ $(gcc-major-version) -lt 4
			|| ( $(gcc-major-version) -eq 4 && $(gcc-minor-version) -lt 6 ) ]]
		then
			eerror "systemd requires at least gcc 4.6 to build. Please switch the active"
			eerror "gcc version using gcc-config."
			die "systemd requires at least gcc 4.6"
		fi
	fi

	if [[ ${MERGE_TYPE} != buildonly ]]; then
		if kernel_is -lt ${MINKV//./ }; then
			ewarn "Kernel version at least ${MINKV} required"
		fi

		if ! use firmware-loader && kernel_is -lt 3 8; then
			ewarn "You seem to be using kernel older than 3.8. Those kernel versions"
			ewarn "require systemd with USE=firmware-loader to support loading"
			ewarn "firmware. Missing this flag may cause some hardware not to work."
		fi

		check_extra_config
	fi
}

pkg_setup() {
	use python && python-single-r1_pkg_setup
}

src_configure() {
	local myeconfargs=(
		--localstatedir=/var
		--with-pamlibdir=$(getpam_mod_dir)
		# avoid bash-completion dep
		--with-bashcompletiondir="$(get_bashcompdir)"
		# make sure we get /bin:/sbin in $PATH
		--enable-split-usr
		# disable sysv compatibility
		--with-sysvinit-path=
		--with-sysvrcnd-path=
		# no deps
		--enable-efi
		--enable-ima
		# optional components/dependencies
		$(use_enable acl)
		$(use_enable audit)
		$(use_enable cryptsetup libcryptsetup)
		$(use_enable doc gtk-doc)
		$(use_enable gcrypt)
		$(use_enable gudev)
		$(use_enable http microhttpd)
		$(use_enable introspection)
		$(use_enable keymap)
		$(use_enable kmod)
		$(use_enable lzma xz)
		$(use_enable pam)
		$(use_enable policykit polkit)
		$(use_with python)
		$(use python && echo PYTHON_CONFIG=/usr/bin/python-config-${EPYTHON#python})
		$(use_enable qrcode qrencode)
		$(use_enable selinux)
		$(use_enable tcpd tcpwrap)
		$(use_enable test tests)
		$(use_enable xattr)

		# not supported (avoid automagic deps in the future)
		--disable-chkconfig

		# hardcode a few paths to spare some deps
		QUOTAON=/usr/sbin/quotaon
		QUOTACHECK=/usr/sbin/quotacheck
	)

	# Keep using the one where the rules were installed.
	MY_UDEVDIR=$(get_udevdir)

	if use firmware-loader; then
		myeconfargs+=(
			--with-firmware-path="/lib/firmware/updates:/lib/firmware"
		)
	fi

	# Work around bug 463846.
	tc-export CC

	autotools-utils_src_configure
}

src_compile() {
	autotools-utils_src_compile \
		udevlibexecdir="${MY_UDEVDIR}"
}

src_install() {
	autotools-utils_src_install -j1 \
		udevlibexecdir="${MY_UDEVDIR}" \
		dist_udevhwdb_DATA=

	# keep udev working without initramfs, for openrc compat
	dodir /bin /sbin
	mv "${D}"/usr/lib/systemd/systemd-udevd "${D}"/sbin/udevd || die
	mv "${D}"/usr/bin/udevadm "${D}"/bin/udevadm || die
	dosym ../../../sbin/udevd /usr/lib/systemd/systemd-udevd
	dosym ../../bin/udevadm /usr/bin/udevadm

	# zsh completion
	insinto /usr/share/zsh/site-functions
	newins shell-completion/systemd-zsh-completion.zsh "_${PN}"

	# compat for init= use
	dosym ../usr/lib/systemd/systemd /bin/systemd
	dosym ../lib/systemd/systemd /usr/bin/systemd
	# rsyslog.service depends on it...
	dosym ../usr/bin/systemctl /bin/systemctl

	# we just keep sysvinit tools, so no need for the mans
	rm "${D}"/usr/share/man/man8/{halt,poweroff,reboot,runlevel,shutdown,telinit}.8 \
		|| die
	rm "${D}"/usr/share/man/man1/init.1 || die

	if ! use vanilla; then
		# Create /run/lock as required by new baselay/OpenRC compat.
		systemd_dotmpfilesd "${FILESDIR}"/gentoo-run.conf

		# Add mount-rules for /var/lock and /var/run, bug #433607
		systemd_dounit "${FILESDIR}"/var-{lock,run}.mount
		systemd_enable_service sysinit.target var-lock.mount
		systemd_enable_service sysinit.target var-run.mount
	fi

	# Disable storing coredumps in journald, bug #433457
	mv "${D}"/usr/lib/sysctl.d/50-coredump.conf{,.disabled} || die

	# Preserve empty dirs in /etc & /var, bug #437008
	keepdir /etc/binfmt.d /etc/modules-load.d /etc/tmpfiles.d \
		/etc/systemd/ntp-units.d /etc/systemd/user /var/lib/systemd

	# Check whether we won't break user's system.
	local x
	for x in /bin/systemd /usr/bin/systemd \
		/usr/bin/udevadm /usr/lib/systemd/systemd-udevd
	do
		[[ -x ${D}${x} ]] || die "${x} symlink broken, aborting."
	done
}

pkg_postinst() {
	# for udev rules
	enewgroup dialout

	enewgroup systemd-journal
	if use http; then
		enewgroup systemd-journal-gateway
		enewuser systemd-journal-gateway -1 -1 -1 systemd-journal-gateway
	fi
	systemd_update_catalog

	# Keep this here in case the database format changes so it gets updated
	# when required. Despite that this file is owned by sys-apps/hwids.
	if has_version "sys-apps/hwids[udev]"; then
		udevadm hwdb --update --root="${ROOT%/}"
	fi

	if [[ ! -L "${ROOT}"/etc/mtab ]]; then
		ewarn "Upstream mandates the /etc/mtab file should be a symlink to /proc/mounts."
		ewarn "Not having it is not supported by upstream and will cause tools like 'df'"
		ewarn "and 'mount' to not work properly. Please run:"
		ewarn "	# ln -sf '${ROOT}proc/self/mounts' '${ROOT}etc/mtab'"
		ewarn
	fi

	if ! has_version sys-apps/systemd-ui; then
		elog "To get additional features, a number of optional runtime dependencies may"
		elog "be installed:"
		elog "- sys-apps/systemd-ui: for GTK+ systemadm UI and gnome-ask-password-agent"
	fi
}

pkg_prerm() {
	# If removing systemd completely, remove the catalog database.
	if [[ ! ${REPLACED_BY_VERSION} ]]; then
		rm -f -v "${EROOT}"/var/lib/systemd/catalog/database
	fi
}
