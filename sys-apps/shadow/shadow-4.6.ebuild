# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit libtool pam multilib systemd

DESCRIPTION="Utilities to deal with user accounts"
HOMEPAGE="https://github.com/shadow-maint/shadow http://pkg-shadow.alioth.debian.org/"
SRC_URI="https://github.com/shadow-maint/shadow/releases/download/${PV}/${P}.tar.gz"

LICENSE="BSD GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm arm64 hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86"
IUSE="acl audit +cracklib nls pam selinux skey xattr"
# Taken from the man/Makefile.am file.
LANGS=( cs da de es fi fr hu id it ja ko pl pt_BR ru sv tr zh_CN zh_TW )

RDEPEND="acl? ( sys-apps/acl:0= )
	audit? ( >=sys-process/audit-2.6:0= )
	cracklib? ( >=sys-libs/cracklib-2.7-r3:0= )
	pam? ( virtual/pam:0= )
	skey? ( sys-auth/skey:0= )
	selinux? (
		>=sys-libs/libselinux-1.28:0=
		sys-libs/libsemanage:0=
	)
	nls? ( virtual/libintl )
	xattr? ( sys-apps/attr:0= )"
DEPEND="${RDEPEND}
	app-arch/xz-utils
	nls? ( sys-devel/gettext )"
RDEPEND="${RDEPEND}
	pam? ( >=sys-auth/pambase-20150213 )"

PATCHES=(
	"${FILESDIR}/${PN}-4.1.3-dots-in-usernames.patch"
)

src_prepare() {
	default
	#eautoreconf
	elibtoolize
}

src_configure() {
	local myeconfargs=(
		--without-group-name-max-length
		--without-tcb
		--enable-shared=no
		--enable-static=yes
		$(use_with acl)
		$(use_with audit)
		$(use_with cracklib libcrack)
		$(use_with pam libpam)
		$(use_with skey)
		$(use_with selinux)
		$(use_enable nls)
		$(use_with elibc_glibc nscd)
		$(use_with xattr attr)
	)
	econf "${myeconfargs[@]}"

	has_version 'sys-libs/uclibc[-rpc]' && sed -i '/RLOGIN/d' config.h #425052

	if use nls ; then
		local l langs="po" # These are the pot files.
		for l in ${LANGS[*]} ; do
			has ${l} ${LINGUAS-${l}} && langs+=" ${l}"
		done
		sed -i "/^SUBDIRS = /s:=.*:= ${langs}:" man/Makefile || die
	fi
}

set_login_opt() {
	local comment="" opt=$1 val=$2
	if [[ -z ${val} ]]; then
		comment="#"
		sed -i \
			-e "/^${opt}\>/s:^:#:" \
			"${ED%/}"/usr/share/shadow/login.defs || die
	else
		sed -i -r \
			-e "/^#?${opt}\>/s:.*:${opt} ${val}:" \
			"${ED%/}"/usr/share/shadow/login.defs
	fi
	local res=$(grep "^${comment}${opt}\>" "${ED%/}"/usr/share/shadow/login.defs)
	einfo "${res:-Unable to find ${opt} in /usr/share/shadow/login.defs}"
}

src_install() {
	emake DESTDIR="${D}" suidperms=4711 install

	# Remove libshadow and libmisc; see bug 37725 and the following
	# comment from shadow's README.linux:
	#   Currently, libshadow.a is for internal use only, so if you see
	#   -lshadow in a Makefile of some other package, it is safe to
	#   remove it.
	rm -f "${ED%/}"/{,usr/}$(get_libdir)/lib{misc,shadow}.{a,la}

	# Remove files from /etc, they will be symlinks to /usr instead.
	rm -f "${ED%/}"/etc/{limits,login.access,login.defs,securetty,default/useradd}

	# CoreOS: break shadow.conf into two files so that we only have to apply
	# etc-shadow.conf in the initrd.
	systemd_dotmpfilesd "${FILESDIR}"/tmpfiles.d/etc-shadow.conf
	systemd_dotmpfilesd "${FILESDIR}"/tmpfiles.d/var-shadow.conf
	# Package the symlinks for the SDK and containers.
	systemd-tmpfiles --create --root="${ED}" "${FILESDIR}"/tmpfiles.d/*

	insinto /usr/share/shadow
	if ! use pam ; then
		insopts -m0600
		doins etc/login.access etc/limits
	fi
	# Using a securetty with devfs device names added
	# (compat names kept for non-devfs compatibility)
	insopts -m0600 ; doins "${FILESDIR}"/securetty
	# Output arch-specific cruft
	local devs
	case $(tc-arch) in
		ppc*)  devs="hvc0 hvsi0 ttyPSC0";;
		hppa)  devs="ttyB0";;
		arm)   devs="ttyFB0 ttySAC0 ttySAC1 ttySAC2 ttySAC3 ttymxc0 ttymxc1 ttymxc2 ttymxc3 ttyO0 ttyO1 ttyO2";;
		sh)    devs="ttySC0 ttySC1";;
		amd64|x86)      devs="hvc0";;
	esac
	if [[ -n ${devs} ]]; then
		printf '%s\n' ${devs} >> "${ED%/}"/usr/share/shadow/securetty
	fi

	# needed for 'useradd -D'
	insopts -m0600
	doins "${FILESDIR}"/default/useradd

	insopts -m0644
	newins etc/login.defs login.defs

	set_login_opt CREATE_HOME yes
	if ! use pam ; then
		set_login_opt MAIL_CHECK_ENAB no
		set_login_opt SU_WHEEL_ONLY yes
		set_login_opt CRACKLIB_DICTPATH /usr/$(get_libdir)/cracklib_dict
		set_login_opt LOGIN_RETRIES 3
		set_login_opt ENCRYPT_METHOD SHA512
		set_login_opt CONSOLE
	else
		dopamd "${FILESDIR}"/pam.d-include/shadow

		for x in chpasswd chgpasswd newusers; do
			newpamd "${FILESDIR}"/pam.d-include/passwd ${x}
		done

		for x in chage chsh chfn \
				 user{add,del,mod} group{add,del,mod} ; do
			newpamd "${FILESDIR}"/pam.d-include/shadow ${x}
		done

		# comment out login.defs options that pam hates
		local opt sed_args=()
		for opt in \
			CHFN_AUTH \
			CONSOLE \
			CRACKLIB_DICTPATH \
			ENV_HZ \
			ENVIRON_FILE \
			FAILLOG_ENAB \
			FTMP_FILE \
			LASTLOG_ENAB \
			MAIL_CHECK_ENAB \
			MOTD_FILE \
			NOLOGINS_FILE \
			OBSCURE_CHECKS_ENAB \
			PASS_ALWAYS_WARN \
			PASS_CHANGE_TRIES \
			PASS_MIN_LEN \
			PORTTIME_CHECKS_ENAB \
			QUOTAS_ENAB \
			SU_WHEEL_ONLY
		do
			set_login_opt ${opt}
			sed_args+=( -e "/^#${opt}\>/b pamnote" )
		done
		sed -i "${sed_args[@]}" \
			-e 'b exit' \
			-e ': pamnote; i# NOTE: This setting should be configured via /etc/pam.d/ and not in this file.' \
			-e ': exit' \
			"${ED%/}"/usr/share/shadow/login.defs || die

		# remove manpages that pam will install for us
		# and/or don't apply when using pam
		find "${ED%/}"/usr/share/man \
			'(' -name 'limits.5*' -o -name 'suauth.5*' ')' \
			-delete

		# Remove pam.d files provided by pambase.
		rm "${ED%/}"/etc/pam.d/{login,passwd,su} || die
	fi

	# Remove manpages that are handled by other packages
	find "${ED%/}"/usr/share/man \
		'(' -name id.1 -o -name passwd.5 -o -name getspnam.3 ')' \
		-delete

	cd "${S}" || die
	dodoc ChangeLog NEWS TODO
	newdoc README README.download
	cd doc || die
	dodoc HOWTO README* WISHLIST *.txt
}

pkg_preinst() {
	rm -f "${EROOT}"/etc/pam.d/system-auth.new \
		"${EROOT}/etc/login.defs.new"
}
