# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

CROS_WORKON_PROJECT="coreos/systemd"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	# Use ~arch instead of empty keywords for compatibility with cros-workon
	KEYWORDS="~amd64 ~arm64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="86c388465b686bb23cede68c226d00f5ff31c3b3" # v234-coreos
	KEYWORDS="~alpha amd64 ~arm arm64 ~ia64 ~ppc ~ppc64 ~sparc ~x86"
fi

PYTHON_COMPAT=( python{2_7,3_4,3_5,3_6} )

# cros-workon must be imported first, in cases where cros-workon and
# another eclass exports the same function (say src_compile) we want
# the later eclass's version to win. Only need src_unpack from workon.
inherit cros-workon

inherit autotools bash-completion-r1 linux-info multilib-minimal pam python-any-r1 systemd toolchain-funcs udev user

DESCRIPTION="System and service manager for Linux"
HOMEPAGE="https://www.freedesktop.org/wiki/Software/systemd"

LICENSE="GPL-2 LGPL-2.1 MIT public-domain"
SLOT="0/2"
IUSE="acl apparmor audit build cryptsetup curl elfutils +gcrypt gnuefi http
	idn importd +kmod libidn2 +lz4 lzma nat pam policykit
	qrcode +seccomp selinux ssl sysv-utils test vanilla xkb"

# CoreOS specific use flags
IUSE+=" symlink-usr"

REQUIRED_USE="importd? ( curl gcrypt lzma )"

MINKV="3.11"

COMMON_DEPEND=">=sys-apps/util-linux-2.27.1:0=[${MULTILIB_USEDEP}]
	sys-libs/libcap:0=[${MULTILIB_USEDEP}]
	!<sys-libs/glibc-2.16
	acl? ( sys-apps/acl:0= )
	apparmor? ( sys-libs/libapparmor:0= )
	audit? ( >=sys-process/audit-2:0= )
	cryptsetup? ( >=sys-fs/cryptsetup-1.6:0= )
	curl? ( net-misc/curl:0= )
	elfutils? ( >=dev-libs/elfutils-0.158:0= )
	gcrypt? ( >=dev-libs/libgcrypt-1.4.5:0=[${MULTILIB_USEDEP}] )
	http? (
		>=net-libs/libmicrohttpd-0.9.33:0=
		ssl? ( >=net-libs/gnutls-3.1.4:0= )
	)
	idn? (
		libidn2? ( net-dns/libidn2 )
		!libidn2? ( net-dns/libidn )
	)
	importd? (
		app-arch/bzip2:0=
		sys-libs/zlib:0=
	)
	kmod? ( >=sys-apps/kmod-15:0= )
	lz4? ( >=app-arch/lz4-0_p131:0=[${MULTILIB_USEDEP}] )
	lzma? ( >=app-arch/xz-utils-5.0.5-r1:0=[${MULTILIB_USEDEP}] )
	nat? ( net-firewall/iptables:0= )
	pam? ( virtual/pam:=[${MULTILIB_USEDEP}] )
	qrcode? ( media-gfx/qrencode:0= )
	seccomp? ( >=sys-libs/libseccomp-2.3.1:0= )
	selinux? ( sys-libs/libselinux:0= )
	sysv-utils? (
		!sys-apps/systemd-sysv-utils
		!sys-apps/sysvinit )
	xkb? ( >=x11-libs/libxkbcommon-0.4.1:0= )
	abi_x86_32? ( !<=app-emulation/emul-linux-x86-baselibs-20130224-r9
		!app-emulation/emul-linux-x86-baselibs[-abi_x86_32(-)] )"

RDEPEND="${COMMON_DEPEND}
	!build? ( || (
		sys-apps/util-linux[kill(-)]
		sys-process/procps[kill(+)]
		sys-apps/coreutils[kill(-)]
	) )
	!sys-auth/nss-myhostname
	!<sys-kernel/dracut-044
	!sys-fs/eudev
	!sys-fs/udev"

# sys-apps/dbus: the daemon only (+ build-time lib dep for tests)
PDEPEND=">=sys-apps/dbus-1.9.8[systemd]
	>=sys-apps/hwids-20150417[udev]
	policykit? ( sys-auth/polkit )
	!vanilla? ( sys-apps/gentoo-systemd-integration )"

# Newer linux-headers needed by ia64, bug #480218
DEPEND="${COMMON_DEPEND}
	app-arch/xz-utils:0
	dev-util/gperf
	>=dev-util/intltool-0.50
	>=sys-apps/coreutils-8.16
	>=sys-kernel/linux-headers-${MINKV}
	virtual/pkgconfig
	gnuefi? ( >=sys-boot/gnu-efi-3.0.2 )
	test? ( sys-apps/dbus )
	app-text/docbook-xml-dtd:4.2
	app-text/docbook-xml-dtd:4.5
	app-text/docbook-xsl-stylesheets
	dev-libs/libxslt:0
	$(python_gen_any_dep 'dev-python/lxml[${PYTHON_USEDEP}]')
"

pkg_pretend() {
	if [[ ${MERGE_TYPE} != buildonly ]]; then
		local CONFIG_CHECK="~AUTOFS4_FS ~BLK_DEV_BSG ~CGROUPS
			~CHECKPOINT_RESTORE ~DEVTMPFS ~EPOLL ~FANOTIFY ~FHANDLE
			~INOTIFY_USER ~IPV6 ~NET ~NET_NS ~PROC_FS ~SIGNALFD ~SYSFS
			~TIMERFD ~TMPFS_XATTR ~UNIX
			~CRYPTO_HMAC ~CRYPTO_SHA256 ~CRYPTO_USER_API_HASH
			~!FW_LOADER_USER_HELPER ~!GRKERNSEC_PROC ~!IDE ~!SYSFS_DEPRECATED
			~!SYSFS_DEPRECATED_V2"

		use acl && CONFIG_CHECK+=" ~TMPFS_POSIX_ACL"
		use seccomp && CONFIG_CHECK+=" ~SECCOMP ~SECCOMP_FILTER"
		kernel_is -lt 3 7 && CONFIG_CHECK+=" ~HOTPLUG"
		kernel_is -lt 4 7 && CONFIG_CHECK+=" ~DEVPTS_MULTIPLE_INSTANCES"

		if linux_config_exists; then
			local uevent_helper_path=$(linux_chkconfig_string UEVENT_HELPER_PATH)
			if [[ -n ${uevent_helper_path} ]] && [[ ${uevent_helper_path} != '""' ]]; then
				ewarn "It's recommended to set an empty value to the following kernel config option:"
				ewarn "CONFIG_UEVENT_HELPER_PATH=${uevent_helper_path}"
			fi
			if linux_chkconfig_present X86; then
				CONFIG_CHECK+=" ~DMIID"
			fi
		fi

		if kernel_is -lt ${MINKV//./ }; then
			ewarn "Kernel version at least ${MINKV} required"
		fi

		check_extra_config
	fi
}

pkg_setup() {
	:
}

src_unpack() {
	default
	cros-workon_src_unpack
}

src_prepare() {
	# Bug 463376
	sed -i -e 's/GROUP="dialout"/GROUP="uucp"/' rules/*.rules || die
	# Use the resolv.conf managed by systemd-resolved
	sed -i -e 's,/usr/lib/systemd/resolv.conf,/run/systemd/resolve/resolv.conf,' tmpfiles.d/etc.conf.m4 || die

	[[ -d "${WORKDIR}"/patches ]] && PATCHES+=( "${WORKDIR}"/patches )

	default

	eautoreconf
}

src_configure() {
	# Keep using the one where the rules were installed.
	MY_UDEVDIR=$(get_udevdir)
	# Fix systems broken by bug #509454.
	[[ ${MY_UDEVDIR} ]] || MY_UDEVDIR=/lib/udev

	# Prevent conflicts with i686 cross toolchain, bug 559726
	tc-export AR CC NM OBJCOPY RANLIB

	python_setup

	multilib-minimal_src_configure
}

multilib_src_configure() {
	local myeconfargs=(
		# disable -flto since it is an optimization flag
		# and makes distcc less effective
		cc_cv_CFLAGS__flto=no
		# disable -fuse-ld=gold since Gentoo supports explicit linker
		# choice and forcing gold is undesired, #539998
		# ld.gold may collide with user's LDFLAGS, #545168
		# ld.gold breaks sparc, #573874
		cc_cv_LDFLAGS__Wl__fuse_ld_gold=no

		# Workaround for gcc-4.7, bug 554454.
		cc_cv_CFLAGS__Werror_shadow=no

		# Workaround for bug 516346
		--enable-dependency-tracking

		--disable-maintainer-mode
		--localstatedir=/var
		--with-pamlibdir=$(getpam_mod_dir)
		# avoid bash-completion dep
		--with-bashcompletiondir="$(get_bashcompdir)"
		# make sure we get /bin:/sbin in $PATH
		--enable-split-usr
		# For testing.
		--with-rootprefix="${ROOTPREFIX-/usr}"
		--with-rootlibdir="${ROOTPREFIX-/usr}/$(get_libdir)"
		# disable sysv compatibility
		--with-sysvinit-path=
		--with-sysvrcnd-path=
		# no deps
		--enable-efi
		--enable-ima

		# Optional components/dependencies
		$(multilib_native_use_enable acl)
		$(multilib_native_use_enable apparmor)
		$(multilib_native_use_enable audit)
		$(multilib_native_use_enable cryptsetup libcryptsetup)
		$(multilib_native_use_enable curl libcurl)
		$(multilib_native_use_enable elfutils)
		$(use_enable gcrypt)
		$(multilib_native_use_enable gnuefi)
		--with-efi-libdir="/usr/$(get_libdir)"
		$(multilib_native_use_enable http microhttpd)
		$(usex http $(multilib_native_use_enable ssl gnutls) --disable-gnutls)
		$(multilib_native_use_enable idn libidn)
		$(multilib_native_use_enable importd)
		$(multilib_native_use_enable importd bzip2)
		$(multilib_native_use_enable importd zlib)
		$(multilib_native_use_enable kmod)
		$(use_enable lz4)
		$(use_enable lzma xz)
		$(multilib_native_use_enable nat libiptc)
		$(use_enable pam)
		$(multilib_native_use_enable policykit polkit)
		$(multilib_native_use_enable qrcode qrencode)
		$(multilib_native_use_enable seccomp)
		$(multilib_native_use_enable selinux)
		$(multilib_native_use_enable test tests)
		$(multilib_native_use_enable test dbus)
		$(multilib_native_use_enable xkb xkbcommon)
		--without-python

		# hardcode a few paths to spare some deps
		KILL=/bin/kill
		QUOTAON=/usr/sbin/quotaon
		QUOTACHECK=/usr/sbin/quotacheck

		# TODO: we may need to restrict this to gcc
		EFI_CC="$(tc-getCC)"

		# dbus paths
		--with-dbussessionservicedir="${EPREFIX}/usr/share/dbus-1/services"
		--with-dbussystemservicedir="${EPREFIX}/usr/share/dbus-1/system-services"

		--with-ntp-servers="0.coreos.pool.ntp.org 1.coreos.pool.ntp.org 2.coreos.pool.ntp.org 3.coreos.pool.ntp.org"

		--with-pamconfdir=/usr/share/pam.d

		# The CoreOS epoch, Mon Jul  1 00:00:00 UTC 2013. Used by timesyncd
		# as a sanity check for the minimum acceptable time. Explicitly set
		# to avoid using the current build time.
		--with-time-epoch=1372636800

		# no default name servers
		--with-dns-servers=

		# Breaks Docker
		--with-default-hierarchy=legacy

		# Breaks screen, tmux, etc.
		--without-kill-user-processes
	)

	# Work around bug 463846.
	tc-export CC

	ECONF_SOURCE="${S}" econf "${myeconfargs[@]}"
}

multilib_src_compile() {
	local mymakeopts=(
		udevlibexecdir="${MY_UDEVDIR}"
	)

	if multilib_is_native_abi; then
		emake "${mymakeopts[@]}"
	else
		emake built-sources
		local targets=(
			'$(rootlib_LTLIBRARIES)'
			'$(lib_LTLIBRARIES)'
			'$(pamlib_LTLIBRARIES)'
			'$(pkgconfiglib_DATA)'
		)
		echo "gentoo: ${targets[*]}" | emake "${mymakeopts[@]}" -f Makefile -f - gentoo
	fi
}

multilib_src_test() {
	multilib_is_native_abi || return 0
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
			install-rootlibLTLIBRARIES
			install-libLTLIBRARIES
			install-pamlibLTLIBRARIES
			install-pkgconfiglibDATA
			install-includeHEADERS
			install-pkgincludeHEADERS
		)

		emake "${mymakeopts[@]}"
	fi
}

multilib_src_install_all() {
	local unitdir=$(systemd_get_systemunitdir)

	prune_libtool_files --modules
	einstalldocs

	if use sysv-utils; then
		local prefix
		use symlink-usr && prefix=/usr
		for app in halt poweroff reboot runlevel shutdown telinit; do
			dosym "${ROOTPREFIX-/usr}/bin/systemctl" ${prefix}/sbin/${app}
		done
		dosym "${ROOTPREFIX-/usr}/lib/systemd/systemd" ${prefix}/sbin/init
	else
		# we just keep sysvinit tools, so no need for the mans
		rm "${ED%/}"/usr/share/man/man8/{halt,poweroff,reboot,runlevel,shutdown,telinit}.8 \
			|| die
		rm "${ED%/}"/usr/share/man/man1/init.1 || die
	fi

	# Ensure journal directory has correct ownership/mode in inital image.
	# This is fixed by systemd-tmpfiles *but* journald starts before that
	# and will create the journal if the filesystem is already read-write.
	# Conveniently the systemd Makefile sets this up completely wrong.
	dodir /var/log/journal
	fowners root:systemd-journal /var/log/journal
	fperms 2755 /var/log/journal

	systemd_dotmpfilesd "${FILESDIR}"/systemd-coreos.conf
	systemd_dotmpfilesd "${FILESDIR}"/systemd-resolv.conf

	# Don't default to graphical.target
	rm "${ED%/}${unitdir}"/default.target || die
	dosym multi-user.target "${unitdir}"/default.target

	# Don't set any extra environment variables by default
	rm "${ED%/}${ROOTPREFIX-/usr}/lib/environment.d/99-environment.conf" || die

	# Don't install the compatibility policy rules in /var  (Fixed: @37377227)
	rm "${ED%/}"/var/lib/polkit-1/localauthority/10-vendor.d/systemd-networkd.pkla
	rmdir "${ED%/}"/var/lib/polkit-1{/localauthority{/10-vendor.d,},}

	# Move a few services enabled in /etc to /usr, delete files individually
	# so builds fail if systemd adds any new unexpected stuff to /etc
	local f
	for f in \
		getty.target.wants/getty@tty1.service \
		multi-user.target.wants/machines.target \
		multi-user.target.wants/remote-fs.target \
		multi-user.target.wants/systemd-networkd.service \
		multi-user.target.wants/systemd-resolved.service \
		network-online.target.wants/systemd-networkd-wait-online.service \
		sockets.target.wants/systemd-networkd.socket \
		sysinit.target.wants/systemd-timesyncd.service
	do
		local s="${f#*/}" t="${f%/*}"
		local u="${s/@*.service/@.service}"

		# systemd_enable_service doesn't understand template units
		einfo "Enabling ${s} via ${t}"
		dodir "${unitdir}/${t}"
		dosym "../${u}" "${unitdir}/${t}/${s}"

		rm "${ED%/}/etc/systemd/system/${f}" || die
	done
	rmdir "${ED%/}"/etc/systemd/system/*.wants || die
	for f in \
		systemd-networkd.service:dbus-org.freedesktop.network1.service \
		systemd-resolved.service:dbus-org.freedesktop.resolve1.service
	do
		rm "${ED%/}/etc/systemd/system/${f#*:}" || die
		dosym "${f%%:*}" "${unitdir}/${f#*:}"
	done

	# Do not enable random services if /etc was detected as empty!!!
	rm "${ED%/}"/usr/lib/systemd/system-preset/90-systemd.preset
	insinto /usr/lib/systemd/system-preset
	doins "${FILESDIR}"/99-default.preset

	# Disable the "First Boot Wizard" by default, it isn't very applicable to CoreOS
	rm "${ED%/}${unitdir}"/sysinit.target.wants/systemd-firstboot.service

	# Do not ship distro-specific files (nsswitch.conf pam.d)
	rm -rf "${ED%/}"/usr/share/factory
	sed -i "${ED%/}"/usr/lib/tmpfiles.d/etc.conf \
		-e '/^C \/etc\/nsswitch\.conf/d' \
		-e '/^C \/etc\/pam\.d/d'
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

pkg_preinst() {
	# If /lib/systemd and /usr/lib/systemd are the same directory, remove the
	# symlinks we created in src_install.
	if [[ $(realpath "${EROOT%/}${ROOTPREFIX}/lib/systemd") == $(realpath "${EROOT%/}/usr/lib/systemd") ]]; then
		if [[ -L ${ED%/}/usr/lib/systemd/systemd ]]; then
			rm "${ED%/}/usr/lib/systemd/systemd" || die
		fi
		if [[ -L ${ED%/}/usr/lib/systemd/systemd-shutdown ]]; then
			rm "${ED%/}/usr/lib/systemd/systemd-shutdown" || die
		fi
	fi
}

pkg_postinst() {
	newusergroup() {
		enewgroup "$1"
		enewuser "$1" -1 -1 -1 "$1"
	}

	enewgroup input
	enewgroup kvm 78
	enewgroup systemd-journal
	newusergroup systemd-coredump
	newusergroup systemd-journal-gateway
	newusergroup systemd-journal-remote
	newusergroup systemd-journal-upload
	newusergroup systemd-network
	newusergroup systemd-resolve
	newusergroup systemd-timesync

	systemd_update_catalog

	# Keep this here in case the database format changes so it gets updated
	# when required. Despite that this file is owned by sys-apps/hwids.
	if has_version "sys-apps/hwids[udev]"; then
		udevadm hwdb --update --root="${EROOT%/}"
	fi

	udev_reload || FAIL=1

	# Bug 465468, make sure locales are respect, and ensure consistency
	# between OpenRC & systemd
	migrate_locale

	if [[ ${FAIL} ]]; then
		eerror "One of the postinst commands failed. Please check the postinst output"
		eerror "for errors. You may need to clean up your system and/or try installing"
		eerror "systemd again."
		eerror
	fi
}

pkg_prerm() {
	# If removing systemd completely, remove the catalog database.
	if [[ ! ${REPLACED_BY_VERSION} ]]; then
		rm -f -v "${EROOT}"/var/lib/systemd/catalog/database
	fi
}
