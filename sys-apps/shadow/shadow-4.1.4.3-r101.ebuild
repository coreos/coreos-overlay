# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/shadow/shadow-4.1.4.3.ebuild,v 1.14 2012/08/18 21:28:14 vapier Exp $

inherit eutils libtool toolchain-funcs pam multilib

DESCRIPTION="Utilities to deal with user accounts"
HOMEPAGE="http://shadow.pld.org.pl/ http://pkg-shadow.alioth.debian.org/"
SRC_URI="http://pkg-shadow.alioth.debian.org/releases/${P}.tar.bz2"

LICENSE="BSD GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86"
IUSE="audit cracklib nls pam selinux skey symlink-usr"

RDEPEND="audit? ( sys-process/audit )
	cracklib? ( >=sys-libs/cracklib-2.7-r3 )
	pam? ( virtual/pam )
	skey? ( sys-auth/skey )
	selinux? ( >=sys-libs/libselinux-1.28 )
	nls? ( virtual/libintl )"
DEPEND="${RDEPEND}
	nls? ( sys-devel/gettext )"
RDEPEND="${RDEPEND}
	pam? ( >=sys-auth/pambase-20080219.1 )"

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/${PN}-4.1.4.3-dup-install-targets.patch
	epatch "${FILESDIR}"/${PN}-4.1.4.2-env-reset-keep-locale.patch #283725
	epatch "${FILESDIR}"/${PN}-4.1.3-dots-in-usernames.patch #22920
	epatch "${FILESDIR}"/${PN}-4.1.4.2-groupmod-pam-check.patch #300790
	epatch "${FILESDIR}"/${PN}-4.1.4.2-su_no_sanitize_env.patch #301957
	epatch "${FILESDIR}"/${PN}-4.1.4.2-fix-etc-gshadow-reading.patch #327605
	elibtoolize
	epunt_cxx
}

src_compile() {
	tc-is-cross-compiler && export ac_cv_func_setpgrp_void=yes
	econf \
		--without-group-name-max-length \
		--enable-shared=no \
		--enable-static=yes \
		$(use_with audit) \
		$(use_with cracklib libcrack) \
		$(use_with pam libpam) \
		$(use_with skey) \
		$(use_with selinux) \
		$(use_enable nls) \
		$(use_with elibc_glibc nscd)
	emake || die "compile problem"
}

set_login_opt() {
	local comment="" opt=$1 val=$2
	[[ -z ${val} ]] && comment="#"
	sed -i -r \
		-e "/^#?${opt}/s:.*:${comment}${opt} ${val}:" \
		"${D}"/etc/login.defs
	local res=$(grep "^${comment}${opt}" "${D}"/etc/login.defs)
	einfo ${res:-Unable to find ${opt} in /etc/login.defs}
}

src_install() {
	emake DESTDIR="${D}" suidperms=4711 install || die "install problem"

	# Remove libshadow and libmisc; see bug 37725 and the following
	# comment from shadow's README.linux:
	#   Currently, libshadow.a is for internal use only, so if you see
	#   -lshadow in a Makefile of some other package, it is safe to
	#   remove it.
	rm -f "${D}"/{,usr/}$(get_libdir)/lib{misc,shadow}.{a,la}

	insinto /etc
	# Using a securetty with devfs device names added
	# (compat names kept for non-devfs compatibility)
	insopts -m0600 ; doins "${FILESDIR}"/securetty
	if ! use pam ; then
		insopts -m0600
		doins etc/login.access etc/limits
	fi
	# Output arch-specific cruft
	local devs
	case $(tc-arch) in
		ppc*)  devs="hvc0 hvsi0 ttyPSC0";;
		hppa)  devs="ttyB0";;
		arm)   devs="ttyFB0 ttySAC0 ttySAC1 ttySAC2 ttySAC3 ttymxc0 ttymxc1 ttyO0 ttyO1 ttyO2";;
		sh)    devs="ttySC0 ttySC1";;
	esac
	[[ -n ${devs} ]] && printf '%s\n' ${devs} >> "${D}"/etc/securetty

	# needed for 'useradd -D'
	insinto /etc/default
	insopts -m0600
	doins "${FILESDIR}"/default/useradd

	# move passwd to / to help recover broke systems #64441
	mv "${D}"/usr/bin/passwd "${D}"/bin/
	if ! use symlink-usr ; then
		dosym /bin/passwd /usr/bin/passwd
	fi

	cd "${S}"
	insinto /etc
	insopts -m0644
	newins etc/login.defs login.defs

	if ! use pam ; then
		set_login_opt MAIL_CHECK_ENAB no
		set_login_opt SU_WHEEL_ONLY yes
		set_login_opt CRACKLIB_DICTPATH /usr/$(get_libdir)/cracklib_dict
		set_login_opt LOGIN_RETRIES 3
		set_login_opt ENCRYPT_METHOD SHA512
	else
		dopamd "${FILESDIR}/pam.d-include/"{su,shadow}

		newpamd "${FILESDIR}/login.pamd.3" login

		for x in passwd chpasswd chgpasswd; do
			newpamd "${FILESDIR}"/pam.d-include/passwd ${x} || die
		done

		for x in chage chsh chfn newusers \
				 user{add,del,mod} group{add,del,mod} ; do
			newpamd "${FILESDIR}"/pam.d-include/shadow ${x} || die
		done

		# comment out login.defs options that pam hates
		local opt
		for opt in \
			CHFN_AUTH \
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
		done

		sed -i -f "${FILESDIR}"/login_defs_pam.sed \
			"${D}"/etc/login.defs

		# remove manpages that pam will install for us
		# and/or don't apply when using pam
		find "${D}"/usr/share/man \
			'(' -name 'limits.5*' -o -name 'suauth.5*' ')' \
			-exec rm {} +
	fi

	# Remove manpages that are handled by other packages
	find "${D}"/usr/share/man \
		'(' -name id.1 -o -name passwd.5 -o -name getspnam.3 ')' \
		-exec rm {} +

	cd "${S}"
	dodoc ChangeLog NEWS TODO
	newdoc README README.download
	cd doc
	dodoc HOWTO README* WISHLIST *.txt
}

pkg_preinst() {
	rm -f "${ROOT}"/etc/pam.d/system-auth.new \
		"${ROOT}/etc/login.defs.new"

	use pam && pam_epam_expand "${D}"/etc/pam.d/login
}

pkg_postinst() {
	# Enable shadow groups (we need ROOT=/ here, as grpconv only
	# operate on / ...).
	if [[ ${ROOT} == / && ! -f /etc/gshadow ]] ; then
		if grpck -r 2>/dev/null ; then
			grpconv
		else
			ewarn "Running 'grpck' returned errors.  Please run it by hand, and then"
			ewarn "run 'grpconv' afterwards!"
		fi
	fi

	einfo "The 'adduser' symlink to 'useradd' has been dropped."
}
