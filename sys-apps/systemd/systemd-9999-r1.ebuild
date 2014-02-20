# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/systemd/systemd-9999.ebuild,v 1.86 2014/02/21 15:40:01 zx2c4 Exp $

EAPI=5

#if LIVE
AUTOTOOLS_AUTORECONF=yes
EGIT_REPO_URI="git://anongit.freedesktop.org/${PN}/${PN}
	http://cgit.freedesktop.org/${PN}/${PN}/"

inherit git-r3
#endif

AUTOTOOLS_PRUNE_LIBTOOL_FILES=all
PYTHON_COMPAT=( python{2_7,3_2,3_3} )
inherit autotools-utils bash-completion-r1 fcaps linux-info multilib \
	multilib-minimal pam python-single-r1 systemd toolchain-funcs udev \
	user

DESCRIPTION="System and service manager for Linux"
HOMEPAGE="http://www.freedesktop.org/wiki/Software/systemd"
SRC_URI="http://www.freedesktop.org/software/systemd/${P}.tar.xz"

LICENSE="GPL-2 LGPL-2.1 MIT public-domain"
SLOT="0/1"
KEYWORDS="~amd64 ~arm ~ppc ~ppc64 ~x86"
IUSE="acl audit cryptsetup doc +firmware-loader gcrypt gudev http introspection
	kdbus +kmod networkd lzma pam policykit python qrcode +seccomp selinux tcpd
	test vanilla xattr"

MINKV="3.0"

COMMON_DEPEND=">=sys-apps/util-linux-2.20:0=
	sys-libs/libcap:0=
	acl? ( sys-apps/acl:0= )
	audit? ( >=sys-process/audit-2:0= )
	cryptsetup? ( >=sys-fs/cryptsetup-1.6:0= )
	gcrypt? ( >=dev-libs/libgcrypt-1.4.5:0= )
	gudev? ( dev-libs/glib:2=[${MULTILIB_USEDEP}] )
	http? ( net-libs/libmicrohttpd:0= )
	introspection? ( >=dev-libs/gobject-introspection-1.31.1:0= )
	kmod? ( >=sys-apps/kmod-15:0= )
	lzma? ( app-arch/xz-utils:0=[${MULTILIB_USEDEP}] )
	pam? ( virtual/pam:= )
	python? ( ${PYTHON_DEPS} )
	qrcode? ( media-gfx/qrencode:0= )
	seccomp? ( sys-libs/libseccomp:0= )
	selinux? ( sys-libs/libselinux:0= )
	tcpd? ( sys-apps/tcp-wrappers:0= )
	xattr? ( sys-apps/attr:0= )
	abi_x86_32? ( !<=app-emulation/emul-linux-x86-baselibs-20130224-r9
		!app-emulation/emul-linux-x86-baselibs[-abi_x86_32(-)] )"

# baselayout-2.2 has /run
RDEPEND="${COMMON_DEPEND}
	>=sys-apps/baselayout-2.2
	|| (
		>=sys-apps/util-linux-2.22
		<sys-apps/sysvinit-2.88-r4
	)
	!sys-auth/nss-myhostname
	!<sys-libs/glibc-2.10
	!sys-fs/udev"

# sys-apps/daemon: the daemon only (+ build-time lib dep for tests)
PDEPEND=">=sys-apps/dbus-1.6.8-r1:0
	>=sys-apps/hwids-20130717-r1[udev]
	>=sys-fs/udev-init-scripts-25
	policykit? ( sys-auth/polkit )
	!vanilla? ( sys-apps/gentoo-systemd-integration )"

# Newer linux-headers needed by ia64, bug #480218
DEPEND="${COMMON_DEPEND}
	app-arch/xz-utils:0
	app-text/docbook-xml-dtd:4.2
	app-text/docbook-xsl-stylesheets
	dev-libs/libxslt:0
	dev-util/gperf
	>=dev-util/intltool-0.50
	>=sys-devel/binutils-2.23.1
	>=sys-devel/gcc-4.6
	>=sys-kernel/linux-headers-${MINKV}
	ia64? ( >=sys-kernel/linux-headers-3.9 )
	virtual/pkgconfig
	doc? ( >=dev-util/gtk-doc-1.18 )
	test? ( >=sys-apps/dbus-1.6.8-r1:0 )"

#if LIVE
DEPEND="${DEPEND}
	dev-libs/gobject-introspection
	>=dev-libs/libgcrypt-1.4.5:0"

SRC_URI=
KEYWORDS=
#endif

src_prepare() {
	if use doc; then
		gtkdocize --docdir docs/ || die
	else
		echo 'EXTRA_DIST =' > docs/gtk-doc.make
	fi

	# Bug 463376
	sed -i -e 's/GROUP="dialout"/GROUP="uucp"/' rules/*.rules || die

	autotools-utils_src_prepare
}

pkg_pretend() {
	local CONFIG_CHECK="~AUTOFS4_FS ~BLK_DEV_BSG ~CGROUPS ~DEVTMPFS ~DMIID
		~EPOLL ~FANOTIFY ~FHANDLE ~INOTIFY_USER ~IPV6 ~NET ~PROC_FS
		~SECCOMP ~SIGNALFD ~SYSFS ~TIMERFD
		~!IDE ~!SYSFS_DEPRECATED ~!SYSFS_DEPRECATED_V2
		~!GRKERNSEC_PROC"

	use acl && CONFIG_CHECK+=" ~TMPFS_POSIX_ACL"
	use pam && CONFIG_CHECK+=" ~AUDITSYSCALL"
	use xattr && CONFIG_CHECK+=" ~TMPFS_XATTR"
	kernel_is -lt 3 7 && CONFIG_CHECK+=" ~HOTPLUG"
	use firmware-loader || CONFIG_CHECK+=" ~!FW_LOADER_USER_HELPER"

	if linux_config_exists; then
		local uevent_helper_path=$(linux_chkconfig_string UEVENT_HELPER_PATH)
			if [ -n "${uevent_helper_path}" ] && [ "${uevent_helper_path}" != '""' ]; then
				ewarn "It's recommended to set an empty value to the following kernel config option:"
				ewarn "CONFIG_UEVENT_HELPER_PATH=${uevent_helper_path}"
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

multilib_src_configure() {
	local myeconfargs=(
		--with-pamconfdir=/usr/share/pam.d
		--with-dbuspolicydir=/usr/share/dbus-1/system.d
		--disable-maintainer-mode
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
		# we enable compat libs, for now. hopefully we can drop this flag later
		--enable-compat-libs
		# optional components/dependencies
		$(use_enable acl)
		$(use_enable audit)
		$(use_enable cryptsetup libcryptsetup)
		$(use_enable doc gtk-doc)
		$(use_enable gcrypt)
		$(use_enable gudev)
		$(use_enable http microhttpd)
		$(use_enable introspection)
		$(use_enable kdbus)
		$(use_enable kmod)
		$(use_enable lzma xz)
		$(use_enable networkd)
		$(use_enable pam)
		$(use_enable policykit polkit)
		$(use_enable python python-devel)
		$(use_enable qrcode qrencode)
		$(use_enable seccomp)
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

	if ! multilib_is_native_abi; then
		myeconfargs+=(
			ac_cv_search_cap_init=
			ac_cv_header_sys_capability_h=yes
			DBUS_CFLAGS=' '
			DBUS_LIBS=' '

			--enable-compat-libs
			--disable-acl
			--disable-audit
			--disable-gcrypt
			--disable-gtk-doc
			--disable-introspection
			--disable-kmod
			--disable-libcryptsetup
			--disable-microhttpd
			--disable-networkd
			--disable-pam
			--disable-polkit
			--disable-qrencode
			--disable-seccomp
			--disable-selinux
			--disable-tcpwrap
			--disable-tests
			--disable-xattr
			--disable-xz
			--disable-python-devel
		)
	fi

	# Work around bug 463846.
	tc-export CC

	autotools-utils_src_configure
}

multilib_src_compile() {
	local mymakeopts=(
		udevlibexecdir="${MY_UDEVDIR}"
	)

	if multilib_is_native_abi; then
		emake "${mymakeopts[@]}"
	else
		# prerequisites for gudev
		use gudev && emake src/gudev/gudev{enumtypes,marshal}.{c,h}

		echo 'gentoo: $(BUILT_SOURCES)' | \
		emake "${mymakeopts[@]}" -f Makefile -f - gentoo
		echo 'gentoo: $(lib_LTLIBRARIES) $(pkgconfiglib_DATA)' | \
		emake "${mymakeopts[@]}" -f Makefile -f - gentoo
	fi
}

multilib_src_test() {
	multilib_is_native_abi || continue

	default
}

multilib_src_install() {
	local mymakeopts=(
		# automake fails with parallel libtool relinking
		# https://bugs.gentoo.org/show_bug.cgi?id=491398
		-j1

		udevlibexecdir="${MY_UDEVDIR}"
		dist_udevhwdb_DATA=
		DESTDIR="${D}"
	)

	if multilib_is_native_abi; then
		emake "${mymakeopts[@]}" install
		# Even with --enable-networkd, it's not right to have this running by default
		# when it's unconfigured.
		rm -f "${D}"/etc/systemd/system/multi-user.target.wants/systemd-networkd.service
	else
		mymakeopts+=(
			install-libLTLIBRARIES
			install-pkgconfiglibDATA
			install-includeHEADERS
			# safe to call unconditionally, 'installs' empty list
			install-libgudev_includeHEADERS
			install-pkgincludeHEADERS
		)

		emake "${mymakeopts[@]}"
	fi

	rmdir ${D}/etc/binfmt.d
	rmdir ${D}/etc/sysctl.d
	rmdir ${D}/etc/tmpfiles.d
	rmdir ${D}/etc/modules-load.d
}

multilib_src_install_all() {
	prune_libtool_files --modules
	einstalldocs

	# we just keep sysvinit tools, so no need for the mans
	rm "${D}"/usr/share/man/man8/{halt,poweroff,reboot,runlevel,shutdown,telinit}.8 \
		|| die
	rm "${D}"/usr/share/man/man1/init.1 || die

	# Disable storing coredumps in journald, bug #433457
	mv "${D}"/usr/lib/sysctl.d/50-coredump.conf{,.disabled} || die

	# Preserve empty dir /var, bug #437008
	keepdir /var/lib/systemd
}

migrate_locale() {
	local envd_locale_def="${EROOT%/}/etc/env.d/02locale"
	local envd_locale=( "${EROOT%/}"/etc/env.d/??locale )
	local locale_conf="${EROOT%/}/etc/locale.conf"

	if [[ ! -L ${locale_conf} && ! -e ${locale_conf} ]]; then
		# If locale.conf does not exist...
		if [[ -e ${envd_locale} ]]; then
			# ...either copy env.d/??locale if there's one
			ebegin "Moving ${envd_locale} to ${locale_conf}"
			mv "${envd_locale}" "${locale_conf}"
			eend ${?} || FAIL=1
		else
			# ...or create a dummy default
			ebegin "Creating ${locale_conf}"
			cat > "${locale_conf}" <<-EOF
				# This file has been created by the sys-apps/systemd ebuild.
				# See locale.conf(5) and localectl(1).

				# LANG=${LANG}
			EOF
			eend ${?} || FAIL=1
		fi
	fi

	if [[ ! -L ${envd_locale} ]]; then
		# now, if env.d/??locale is not a symlink (to locale.conf)...
		if [[ -e ${envd_locale} ]]; then
			# ...warn the user that he has duplicate locale settings
			ewarn
			ewarn "To ensure consistent behavior, you should replace ${envd_locale}"
			ewarn "with a symlink to ${locale_conf}. Please migrate your settings"
			ewarn "and create the symlink with the following command:"
			ewarn "ln -s -n -f ../locale.conf ${envd_locale}"
			ewarn
		else
			# ...or just create the symlink if there's nothing here
			ebegin "Creating ${envd_locale_def} -> ../locale.conf symlink"
			ln -n -s ../locale.conf "${envd_locale_def}"
			eend ${?} || FAIL=1
		fi
	fi
}

pkg_postinst() {
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

	udev_reload || FAIL=1

	# Bug 468876
	fcaps cap_dac_override,cap_sys_ptrace=ep usr/bin/systemd-detect-virt

	# Bug 465468, make sure locales are respect, and ensure consistency
	# between OpenRC & systemd
	migrate_locale

	if [[ ${FAIL} ]]; then
		eerror "One of the postinst commands failed. Please check the postinst output"
		eerror "for errors. You may need to clean up your system and/or try installing"
		eerror "systemd again."
		eerror
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
