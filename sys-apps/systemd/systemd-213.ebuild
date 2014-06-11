# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/systemd/systemd-9999.ebuild,v 1.108 2014/05/03 17:35:41 mgorny Exp $

EAPI=5

if [[ ${PV} == 9999 ]]; then
AUTOTOOLS_AUTORECONF=yes
EGIT_REPO_URI="git://anongit.freedesktop.org/${PN}/${PN}
	http://cgit.freedesktop.org/${PN}/${PN}/"

inherit git-r3

elif [[ ${PV} == *9999 ]]; then
AUTOTOOLS_AUTORECONF=yes
EGIT_REPO_URI="git://anongit.freedesktop.org/${PN}/${PN}-stable
	http://cgit.freedesktop.org/${PN}/${PN}-stable/"
EGIT_BRANCH=v${PV%%.*}-stable

inherit git-r3
fi

AUTOTOOLS_PRUNE_LIBTOOL_FILES=all
PYTHON_COMPAT=( python{2_7,3_2,3_3} )
inherit autotools-utils bash-completion-r1 fcaps linux-info multilib \
	multilib-minimal pam python-single-r1 systemd toolchain-funcs udev \
	user

DESCRIPTION="System and service manager for Linux"
HOMEPAGE="http://www.freedesktop.org/wiki/Software/systemd"
SRC_URI="http://www.freedesktop.org/software/systemd/${P}.tar.xz"

LICENSE="GPL-2 LGPL-2.1 MIT public-domain"
SLOT="0/2"
KEYWORDS="~alpha amd64 ~arm ~ia64 ~ppc ~ppc64 ~sparc ~x86"
IUSE="acl audit cryptsetup doc +firmware-loader gcrypt gudev http introspection
	kdbus +kmod lzma pam policykit python qrcode +seccomp selinux ssl
	test vanilla xattr openrc"

MINKV="3.0"

COMMON_DEPEND=">=sys-apps/util-linux-2.20:0=
	sys-libs/libcap:0=
	acl? ( sys-apps/acl:0= )
	audit? ( >=sys-process/audit-2:0= )
	cryptsetup? ( >=sys-fs/cryptsetup-1.6:0= )
	gcrypt? ( >=dev-libs/libgcrypt-1.4.5:0= )
	gudev? ( dev-libs/glib:2=[${MULTILIB_USEDEP}] )
	http? (
		>=net-libs/libmicrohttpd-0.9.33:0=
		ssl? ( >=net-libs/gnutls-3.1.4:0= )
	)
	introspection? ( >=dev-libs/gobject-introspection-1.31.1:0= )
	kmod? ( >=sys-apps/kmod-15:0= )
	lzma? ( app-arch/xz-utils:0=[${MULTILIB_USEDEP}] )
	pam? ( virtual/pam:= )
	python? ( ${PYTHON_DEPS} )
	qrcode? ( media-gfx/qrencode:0= )
	seccomp? ( >=sys-libs/libseccomp-2.1:0= )
	selinux? ( sys-libs/libselinux:0= )
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
	!<sys-libs/glibc-2.14
	!sys-fs/udev"

# sys-apps/daemon: the daemon only (+ build-time lib dep for tests)
PDEPEND=">=sys-apps/dbus-1.6.8-r1:0
	>=sys-apps/hwids-20130717-r1[udev]
	openrc? ( >=sys-fs/udev-init-scripts-25 )
	policykit? ( sys-auth/polkit )"

DEPEND="${COMMON_DEPEND}
	app-arch/xz-utils:0
	dev-util/gperf
	>=dev-util/intltool-0.50
	>=sys-devel/binutils-2.23.1
	>=sys-devel/gcc-4.6
	>=sys-kernel/linux-headers-3.13
	virtual/pkgconfig
	doc? ( >=dev-util/gtk-doc-1.18 )
	python? ( dev-python/lxml[${PYTHON_USEDEP}] )
	test? ( >=sys-apps/dbus-1.6.8-r1:0 )"

if [[ ${PV} == *9999 ]]; then
DEPEND="${DEPEND}
	app-text/docbook-xml-dtd:4.2
	app-text/docbook-xml-dtd:4.5
	app-text/docbook-xsl-stylesheets
	dev-libs/libxslt:0
	dev-libs/gobject-introspection
	>=dev-libs/libgcrypt-1.4.5:0"

SRC_URI=
KEYWORDS=
fi

src_prepare() {
if [[ ${PV} == *9999 ]]; then
	if use doc; then
		gtkdocize --docdir docs/ || die
	else
		echo 'EXTRA_DIST =' > docs/gtk-doc.make
	fi
fi
	# fix regression in systemd-tmpfiles
	epatch "${FILESDIR}"/213-0001-shared-fix-searching-for-configs-in-alternate-roots.patch

	# fix DHCP for VMware bridged network interfaces
	epatch "${FILESDIR}"/213-0002-sd-dhcp-client-Sets-broadcast-flag-to-1.patch

	# Bug 463376
	sed -i -e 's/GROUP="dialout"/GROUP="uucp"/' rules/*.rules || die

	autotools-utils_src_prepare
}

pkg_pretend() {
	local CONFIG_CHECK="~AUTOFS4_FS ~BLK_DEV_BSG ~CGROUPS ~DEVTMPFS ~DMIID
		~EPOLL ~FANOTIFY ~FHANDLE ~INOTIFY_USER ~IPV6 ~NET ~NET_NS ~PROC_FS
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

src_configure() {
	# Keep using the one where the rules were installed.
	MY_UDEVDIR=$(get_udevdir)
	# Fix systems broken by bug #509454.
	[[ ${MY_UDEVDIR} ]] || MY_UDEVDIR=/lib/udev

	multilib-minimal_src_configure
}

multilib_src_configure() {
	local myeconfargs=(
		# disable -flto since it is an optimization flag
		# and makes distcc less effective
		cc_cv_CFLAGS__flto=no

		--with-pamconfdir=/usr/share/pam.d
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
		# optional components/dependencies
		$(use_enable acl)
		$(use_enable audit)
		$(use_enable cryptsetup libcryptsetup)
		$(use_enable doc gtk-doc)
		$(use_enable gcrypt)
		$(use_enable gudev)
		$(use_enable http microhttpd)
		$(usex http $(use_enable ssl gnutls) --disable-gnutls)
		$(use_enable introspection)
		$(use_enable kdbus)
		$(use_enable kmod)
		$(use_enable lzma xz)
		$(use_enable pam)
		$(use_enable policykit polkit)
		$(use_with python)
		$(use_enable python python-devel)
		$(use_enable qrcode qrencode)
		$(use_enable seccomp)
		$(use_enable selinux)
		$(use_enable test tests)
		$(use_enable xattr)

		# not supported (avoid automagic deps in the future)
		--disable-chkconfig

		# hardcode a few paths to spare some deps
		QUOTAON=/usr/sbin/quotaon
		QUOTACHECK=/usr/sbin/quotacheck

		# dbus paths
		--with-dbuspolicydir="${EPREFIX}/usr/share/dbus-1/system.d"
		--with-dbussessionservicedir="${EPREFIX}/usr/share/dbus-1/services"
		--with-dbussystemservicedir="${EPREFIX}/usr/share/dbus-1/system-services"
		--with-dbusinterfacedir="${EPREFIX}/usr/share/dbus-1/interfaces"
	)

	if use firmware-loader; then
		myeconfargs+=(
			--with-firmware-path="/lib/firmware/updates:/lib/firmware"
		)
	fi

	# Added for testing; this is UNSUPPORTED by the Gentoo systemd team!
	if [[ -n ${ROOTPREFIX+set} ]]; then
		myeconfargs+=(
			--with-rootprefix="${ROOTPREFIX}"
			--with-rootlibdir="${ROOTPREFIX}/$(get_libdir)"
		)
	fi

	if ! multilib_is_native_abi; then
		myeconfargs+=(
			ac_cv_search_cap_init=
			ac_cv_header_sys_capability_h=yes
			DBUS_CFLAGS=' '
			DBUS_LIBS=' '

			--disable-acl
			--disable-audit
			--disable-gcrypt
			--disable-gnutls
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

	# install compat pkg-config files
	local pcfiles=( src/compat-libs/libsystemd-{daemon,id128,journal,login}.pc )
	emake "${mymakeopts[@]}" install-pkgconfiglibDATA \
		pkgconfiglib_DATA="${pcfiles[*]}"
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

	systemd_dotmpfilesd "${FILESDIR}"/systemd-coreos.conf

	# Don't default to graphical.target
	rm "${D}"/usr/lib/systemd/system/default.target || die
	dosym multi-user.target /usr/lib/systemd/system/default.target

	# Move a few services enabled in /etc to /usr
	rm "${D}"/etc/systemd/system/getty.target.wants/getty@tty1.service || die
	rmdir "${D}"/etc/systemd/system/getty.target.wants || die
	dosym ../getty@.service /usr/lib/systemd/system/getty.target.wants/getty@tty1.service

	rm "${D}"/etc/systemd/system/multi-user.target.wants/remote-fs.target \
		"${D}"/etc/systemd/system/multi-user.target.wants/systemd-networkd.service \
		"${D}"/etc/systemd/system/multi-user.target.wants/systemd-resolved.service \
		"${D}"/etc/systemd/system/network-online.target.wants/systemd-networkd-wait-online.service \
		|| die
	rmdir "${D}"/etc/systemd/system/multi-user.target.wants \
		"${D}"/etc/systemd/system/network-online.target.wants \
		|| die
	systemd_enable_service multi-user.target remote-fs.target
	systemd_enable_service multi-user.target systemd-networkd.service
	systemd_enable_service multi-user.target systemd-resolved.service
	systemd_enable_service network-online.target systemd-networkd-wait-online.service
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

migrate_net_name_slot() {
	# If user has disabled 80-net-name-slot.rules using a empty file or a symlink to /dev/null,
	# do the same for 80-net-setup-link.rules to keep the old behavior
	local net_move=no
	local net_name_slot_sym=no
	local net_rules_path="${EROOT%/}"/etc/udev/rules.d
	local net_name_slot="${net_rules_path}"/80-net-name-slot.rules
	local net_setup_link="${net_rules_path}"/80-net-setup-link.rules
	if [[ -e ${net_setup_link} ]]; then
		net_move=no
	elif [[ -f ${net_name_slot} && $(sed -e "/^#/d" -e "/^\W*$/d" ${net_name_slot} | wc -l) == 0 ]]; then
		net_move=yes
	elif [[ -L ${net_name_slot} && $(readlink ${net_name_slot}) == /dev/null ]]; then
		net_move=yes
		net_name_slot_sym=yes
	fi
	if [[ ${net_move} == yes ]]; then
		ebegin "Copying ${net_name_slot} to ${net_setup_link}"

		if [[ ${net_name_slot_sym} == yes ]]; then
			ln -nfs /dev/null "${net_setup_link}"
		else
			cp "${net_name_slot}" "${net_setup_link}"
		fi
		eend $? || FAIL=1
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

	# Migrate 80-net-name-slot.rules -> 80-net-setup-link.rules
	migrate_net_name_slot

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
