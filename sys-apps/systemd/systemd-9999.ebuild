# Copyright 2011-2019 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

CROS_WORKON_PROJECT="coreos/systemd"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == 9999 ]]; then
	# Use ~arch instead of empty keywords for compatibility with cros-workon
	KEYWORDS="~amd64 ~arm64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="d1d16e9fb692309580880a4ed99307a560fdeb02" # v242-coreos
	KEYWORDS="~alpha amd64 ~arm arm64 ~ia64 ~ppc ~ppc64 ~sparc ~x86"
fi

PYTHON_COMPAT=( python{3_5,3_6,3_7} )

# cros-workon must be imported first, in cases where cros-workon and
# another eclass exports the same function (say src_compile) we want
# the later eclass's version to win. Only need src_unpack from workon.
inherit cros-workon

inherit bash-completion-r1 linux-info meson multilib-minimal ninja-utils pam python-any-r1 systemd toolchain-funcs udev user

DESCRIPTION="System and service manager for Linux"
HOMEPAGE="https://www.freedesktop.org/wiki/Software/systemd"

LICENSE="GPL-2 LGPL-2.1 MIT public-domain"
SLOT="0/2"
IUSE="acl apparmor audit build cryptsetup curl dns-over-tls elfutils +gcrypt gnuefi gnutls http idn importd +kmod libidn2 +lz4 lzma nat pam pcre policykit qrcode +resolvconf +seccomp selinux +split-usr +sysv-utils test vanilla xkb"

REQUIRED_USE="importd? ( curl gcrypt lzma )"
RESTRICT="!test? ( test )"

MINKV="3.11"

COMMON_DEPEND=">=sys-apps/util-linux-2.30:0=[${MULTILIB_USEDEP}]
	sys-libs/libcap:0=[${MULTILIB_USEDEP}]
	!<sys-libs/glibc-2.16
	acl? ( sys-apps/acl:0= )
	apparmor? ( sys-libs/libapparmor:0= )
	audit? ( >=sys-process/audit-2:0= )
	cryptsetup? ( >=sys-fs/cryptsetup-1.6:0= )
	curl? ( net-misc/curl:0= )
	dns-over-tls? (
		gnutls? ( >=net-libs/gnutls-3.5.3:0= )
		!gnutls? ( >=dev-libs/openssl-1.1.0:0= )
	)
	elfutils? ( >=dev-libs/elfutils-0.158:0= )
	gcrypt? ( >=dev-libs/libgcrypt-1.4.5:0=[${MULTILIB_USEDEP}] )
	http? (
		>=net-libs/libmicrohttpd-0.9.33:0=
		gnutls? ( >=net-libs/gnutls-3.1.4:0= )
	)
	idn? (
		libidn2? ( net-dns/libidn2:= )
		!libidn2? ( net-dns/libidn:= )
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
	pcre? ( dev-libs/libpcre2 )
	qrcode? ( media-gfx/qrencode:0= )
	seccomp? ( >=sys-libs/libseccomp-2.3.3:0= )
	selinux? ( sys-libs/libselinux:0= )
	xkb? ( >=x11-libs/libxkbcommon-0.4.1:0= )"

RDEPEND="${COMMON_DEPEND}
	sysv-utils? ( !sys-apps/sysvinit )
	!sysv-utils? ( sys-apps/sysvinit )
	resolvconf? ( !net-dns/openresolv )
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
DEPEND="
	>=sys-kernel/linux-headers-${MINKV}
	gnuefi? ( >=sys-boot/gnu-efi-3.0.2 )
"

BDEPEND="
	app-arch/xz-utils:0
	dev-util/gperf
	>=dev-util/meson-0.46
	>=dev-util/intltool-0.50
	>=sys-apps/coreutils-8.16
	virtual/pkgconfig[${MULTILIB_USEDEP}]
	test? ( sys-apps/dbus )
	app-text/docbook-xml-dtd:4.2
	app-text/docbook-xml-dtd:4.5
	app-text/docbook-xsl-stylesheets
	dev-libs/libxslt:0
	$(python_gen_any_dep 'dev-python/lxml[${PYTHON_USEDEP}]')
"

pkg_pretend() {
	if [[ ${MERGE_TYPE} != buildonly ]]; then
		if use test && has pid-sandbox ${FEATURES}; then
			ewarn "Tests are known to fail with PID sandboxing enabled."
			ewarn "See https://bugs.gentoo.org/674458."
		fi

		local CONFIG_CHECK="~AUTOFS4_FS ~BLK_DEV_BSG ~CGROUPS
			~CHECKPOINT_RESTORE ~DEVTMPFS ~EPOLL ~FANOTIFY ~FHANDLE
			~INOTIFY_USER ~IPV6 ~NET ~NET_NS ~PROC_FS ~SIGNALFD ~SYSFS
			~TIMERFD ~TMPFS_XATTR ~UNIX
			~CRYPTO_HMAC ~CRYPTO_SHA256 ~CRYPTO_USER_API_HASH
			~!FW_LOADER_USER_HELPER_FALLBACK ~!GRKERNSEC_PROC ~!IDE ~!SYSFS_DEPRECATED
			~!SYSFS_DEPRECATED_V2"

		use acl && CONFIG_CHECK+=" ~TMPFS_POSIX_ACL"
		use seccomp && CONFIG_CHECK+=" ~SECCOMP ~SECCOMP_FILTER"
		kernel_is -lt 3 7 && CONFIG_CHECK+=" ~HOTPLUG"
		kernel_is -lt 4 7 && CONFIG_CHECK+=" ~DEVPTS_MULTIPLE_INSTANCES"
		kernel_is -ge 4 10 && CONFIG_CHECK+=" ~CGROUP_BPF"

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
	# Use the resolv.conf managed by systemd-resolved
	sed -i -e 's,/run/systemd/resolve/stub-resolv.conf,/run/systemd/resolve/resolv.conf,' tmpfiles.d/etc.conf.m4 || die

	default
}

src_configure() {
	# Prevent conflicts with i686 cross toolchain, bug 559726
	tc-export AR CC NM OBJCOPY RANLIB

	python_setup

	multilib-minimal_src_configure
}

meson_use() {
	usex "$1" true false
}

meson_multilib() {
	if multilib_is_native_abi; then
		echo true
	else
		echo false
	fi
}

meson_multilib_native_use() {
	if multilib_is_native_abi && use "$1"; then
		echo true
	else
		echo false
	fi
}

multilib_src_configure() {
	local myconf=(
		--localstatedir="${EPREFIX}/var"
		-Dpamlibdir="$(getpam_mod_dir)"
		# avoid bash-completion dep
		-Dbashcompletiondir="$(get_bashcompdir)"
		# make sure we get /bin:/sbin in PATH
		-Dsplit-usr=$(usex split-usr true false)
		-Drootprefix="$(usex split-usr "${EPREFIX:-/}" "${EPREFIX}/usr")"
		-Dsysvinit-path=
		-Dsysvrcnd-path=
		# Avoid infinite exec recursion, bug 642724
		-Dtelinit-path="${EPREFIX}/lib/sysvinit/telinit"
		# no deps
		-Defi=$(meson_multilib)
		-Dima=true
		# Optional components/dependencies
		-Dacl=$(meson_multilib_native_use acl)
		-Dapparmor=$(meson_multilib_native_use apparmor)
		-Daudit=$(meson_multilib_native_use audit)
		-Dlibcryptsetup=$(meson_multilib_native_use cryptsetup)
		-Dlibcurl=$(meson_multilib_native_use curl)
		-Delfutils=$(meson_multilib_native_use elfutils)
		-Dgcrypt=$(meson_use gcrypt)
		-Dgnu-efi=$(meson_multilib_native_use gnuefi)
		-Dgnutls=$(meson_multilib_native_use gnutls)
		-Defi-libdir="${EPREFIX}/usr/$(get_libdir)"
		-Dmicrohttpd=$(meson_multilib_native_use http)
		-Dimportd=$(meson_multilib_native_use importd)
		-Dbzip2=$(meson_multilib_native_use importd)
		-Dzlib=$(meson_multilib_native_use importd)
		-Dkmod=$(meson_multilib_native_use kmod)
		-Dlz4=$(meson_use lz4)
		-Dxz=$(meson_use lzma)
		-Dlibiptc=$(meson_multilib_native_use nat)
		-Dpam=$(meson_use pam)
		-Dpcre2=$(meson_multilib_native_use pcre)
		-Dpolkit=$(meson_multilib_native_use policykit)
		-Dqrencode=$(meson_multilib_native_use qrcode)
		-Dseccomp=$(meson_multilib_native_use seccomp)
		-Dselinux=$(meson_multilib_native_use selinux)
		-Ddbus=$(meson_multilib_native_use test)
		-Dxkbcommon=$(meson_multilib_native_use xkb)
		# hardcode a few paths to spare some deps
		-Dkill-path=/bin/kill
		-Dntp-servers="0.gentoo.pool.ntp.org 1.gentoo.pool.ntp.org 2.gentoo.pool.ntp.org 3.gentoo.pool.ntp.org"
		# Breaks screen, tmux, etc.
		-Ddefault-kill-user-processes=false

		# multilib options
		-Dbacklight=$(meson_multilib)
		-Dbinfmt=$(meson_multilib)
		-Dcoredump=$(meson_multilib)
		-Denvironment-d=$(meson_multilib)
		-Dfirstboot=$(meson_multilib)
		-Dhibernate=$(meson_multilib)
		-Dhostnamed=$(meson_multilib)
		-Dhwdb=$(meson_multilib)
		-Dldconfig=$(meson_multilib)
		-Dlocaled=$(meson_multilib)
		-Dman=$(meson_multilib)
		-Dnetworkd=$(meson_multilib)
		-Dquotacheck=$(meson_multilib)
		-Drandomseed=$(meson_multilib)
		-Drfkill=$(meson_multilib)
		-Dsysusers=$(meson_multilib)
		-Dtimedated=$(meson_multilib)
		-Dtimesyncd=$(meson_multilib)
		-Dtmpfiles=$(meson_multilib)
		-Dvconsole=$(meson_multilib)

		### CoreOS options

		# Upstream systemd recommends distros explicitly set this
		-Dsplit-bin=true

		# Specify this, or meson breaks due to no /etc/login.defs
		-Dsystem-gid-max=999
		-Dsystem-uid-max=999

		# dbus paths
		-Ddbussessionservicedir="${EPREFIX}/usr/share/dbus-1/services"
		-Ddbussystemservicedir="${EPREFIX}/usr/share/dbus-1/system-services"

		-Dntp-servers="0.coreos.pool.ntp.org 1.coreos.pool.ntp.org 2.coreos.pool.ntp.org 3.coreos.pool.ntp.org"

		-Dpamconfdir=/usr/share/pam.d

		# The CoreOS epoch, Mon Jul  1 00:00:00 UTC 2013. Used by timesyncd
		# as a sanity check for the minimum acceptable time. Explicitly set
		# to avoid using the current build time.
		-Dtime-epoch=1372636800

		# no default name servers
		-Ddns-servers=

		# Breaks Docker
		-Ddefault-hierarchy=legacy

		# Disable the "First Boot Wizard", it isn't very applicable to CoreOS
		-Dfirstboot=false

		# unported options, still needed?
		-Defi-cc="$(tc-getCC)"
		-Dquotaon-path=/usr/sbin/quotaon
		-Dquotacheck-path=/usr/sbin/quotacheck
		-Drootlibdir="${EPREFIX}$(usex split-usr '' /usr)/$(get_libdir)"
	)

	if multilib_is_native_abi && use idn; then
		myconf+=(
			-Dlibidn2=$(usex libidn2 true false)
			-Dlibidn=$(usex libidn2 false true)
		)
	else
		myconf+=(
			-Dlibidn2=false
			-Dlibidn=false
		)
	fi

	if multilib_is_native_abi && use dns-over-tls; then
		myconf+=(
			-Ddns-over-tls=true
			-Dopenssl=$(usex !gnutls true false)
		)
	else
		myconf+=( -Ddns-over-tls=false -Dopenssl=false )
	fi

	meson_src_configure "${myconf[@]}"
}

multilib_src_compile() {
	eninja
}

multilib_src_test() {
	unset DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR
	eninja test
}

multilib_src_install() {
	DESTDIR="${D}" eninja install
}

multilib_src_install_all() {
	local rootprefix=$(usex split-usr '' /usr)

	# meson doesn't know about docdir
	mv "${ED}"/usr/share/doc/{systemd,${PF}} || die

	einstalldocs

	if ! use resolvconf; then
		rm -f "${ED}${rootprefix}"/sbin/resolvconf || die
	fi

	if ! use sysv-utils; then
		rm "${ED}${rootprefix}"/sbin/{halt,init,poweroff,reboot,runlevel,shutdown,telinit} || die
		rm "${ED}"/usr/share/man/man1/init.1 || die
		rm "${ED}"/usr/share/man/man8/{halt,poweroff,reboot,runlevel,shutdown,telinit}.8 || die
	fi

	if ! use resolvconf && ! use sysv-utils; then
		rmdir "${ED}${rootprefix}"/sbin || die
	fi

	local udevdir=/lib/udev
	use split-usr || udevdir=/usr/lib/udev

	rm -r "${ED}${udevdir}/hwdb.d" || die

	if use split-usr; then
		# Avoid breaking boot/reboot
		dosym ../../../lib/systemd/systemd /usr/lib/systemd/systemd
		dosym ../../../lib/systemd/systemd-shutdown /usr/lib/systemd/systemd-shutdown
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
	local unitdir=$(PKG_CONFIG_LIBDIR="${PWD}/src/core" systemd_get_systemunitdir)
	dosym multi-user.target "${unitdir}"/default.target

	# Don't set any extra environment variables by default
	rm "${ED}/usr/lib/environment.d/99-environment.conf" || die

	# Move a few services enabled in /etc to /usr, delete files individually
	# so builds fail if systemd adds any new unexpected stuff to /etc
	local f
	for f in \
		getty.target.wants/getty@tty1.service \
		multi-user.target.wants/machines.target \
		$(usex cryptsetup multi-user.target.wants/remote-cryptsetup.target '') \
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
	done
	rmdir "${ED}"/etc/systemd/system/*.wants || die
	for f in \
		systemd-networkd.service:dbus-org.freedesktop.network1.service \
		systemd-resolved.service:dbus-org.freedesktop.resolve1.service
	do
		dosym "${f%%:*}" "${unitdir}/${f#*:}"
	done

	# Do not enable random services if /etc was detected as empty!!!
	rm "${ED}$(usex split-usr '' /usr)/lib/systemd/system-preset/90-systemd.preset" || die
	insinto $(usex split-usr '' /usr)/lib/systemd/system-preset
	doins "${FILESDIR}"/99-default.preset

	# Do not ship distro-specific files (nsswitch.conf pam.d)
	rm -rf "${ED}"/usr/share/factory
	sed -i "${ED}"/usr/lib/tmpfiles.d/etc.conf \
		-e '/^C \/etc\/nsswitch\.conf/d' \
		-e '/^C \/etc\/pam\.d/d'
}

migrate_locale() {
	local envd_locale_def="${EROOT}/etc/env.d/02locale"
	local envd_locale=( "${EROOT}"/etc/env.d/??locale )
	local locale_conf="${EROOT}/etc/locale.conf"

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
	newusergroup() {
		enewgroup "$1"
		enewuser "$1" -1 -1 -1 "$1"
	}

	enewgroup input
	enewgroup kvm 78
	enewgroup render
	enewgroup systemd-journal
	newusergroup systemd-coredump
	newusergroup systemd-journal-remote
	newusergroup systemd-network
	newusergroup systemd-resolve
	newusergroup systemd-timesync

	systemd_update_catalog

	# Keep this here in case the database format changes so it gets updated
	# when required. Despite that this file is owned by sys-apps/hwids.
	if has_version "sys-apps/hwids[udev]"; then
		udevadm hwdb --update --root="${EROOT}"
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
