# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/dhcpcd/dhcpcd-5.6.4.ebuild,v 1.8 2012/12/15 17:36:38 armin76 Exp $

EAPI=4

inherit eutils systemd

MY_P="${P/_alpha/-alpha}"
MY_P="${MY_P/_beta/-beta}"
MY_P="${MY_P/_rc/-rc}"
S="${WORKDIR}/${MY_P}"

DESCRIPTION="A fully featured, yet light weight RFC2131 compliant DHCP client"
HOMEPAGE="http://roy.marples.name/projects/dhcpcd/"
SRC_URI="http://roy.marples.name/downloads/${PN}/${MY_P}.tar.bz2"
LICENSE="BSD-2"

KEYWORDS="alpha amd64 arm hppa ia64 ~mips ppc ppc64 s390 sh sparc x86 ~amd64-fbsd ~sparc-fbsd ~x86-fbsd ~amd64-linux ~x86-linux"

SLOT="0"
IUSE="+zeroconf elibc_glibc"

DEPEND=""
RDEPEND=""

src_prepare() {
	epatch_user
	if ! use zeroconf; then
		elog "Disabling zeroconf support"
		{
			echo
			echo "# dhcpcd ebuild requested no zeroconf"
			echo "noipv4ll"
		} >> dhcpcd.conf
	fi
	echo 'denyinterfaces docker*' >> dhcpcd.conf
}

src_configure() {
	local hooks="--with-hook=ntp.conf"
	use elibc_glibc && hooks="${hooks} --with-hook=yp.conf"
	econf \
			--prefix="${EPREFIX}" \
			--libexecdir="${EPREFIX}/lib/dhcpcd" \
			--dbdir="${EPREFIX}/var/lib/dhcpcd" \
		--localstatedir="${EPREFIX}/var" \
		${hooks}
}

src_install() {
	default
	newinitd "${FILESDIR}"/${PN}.initd ${PN}
	systemd_dounit "${FILESDIR}"/${PN}.service
}

pkg_preinst() {
	has_version 'net-misc/dhcpcd[zeroconf]' && prev_zero=true || prev_zero=false
}

pkg_postinst() {
	# Upgrade the duid file to the new format if needed
	local old_duid="${ROOT}"/var/lib/dhcpcd/dhcpcd.duid
	local new_duid="${ROOT}"/etc/dhcpcd.duid
	if [ -e "${old_duid}" ] && ! grep -q '..:..:..:..:..:..' "${old_duid}"; then
		sed -i -e 's/\(..\)/\1:/g; s/:$//g' "${old_duid}"
	fi

	# Move the duid to /etc, a more sensible location
	if [ -e "${old_duid}" -a ! -e "${new_duid}" ]; then
		cp -p "${old_duid}" "${new_duid}"
	fi

	if use zeroconf && ! $prev_zero; then
		elog "You have installed dhcpcd with zeroconf support."
		elog "This means that it will always obtain an IP address even if no"
		elog "DHCP server can be contacted, which will break any existing"
		elog "failover support you may have configured in your net configuration."
		elog "This behaviour can be controlled with the -L flag."
		elog "See the dhcpcd man page for more details."
	fi

	elog
	elog "Users upgrading from 4.0 series should pay attention to removal"
	elog "of compat useflag. This changes behavior of dhcp in wide manner:"
	elog "dhcpcd no longer sends a default ClientID for ethernet interfaces."
	elog "This is so we can re-use the address the kernel DHCP client found."
	elog "To retain the old behaviour of sending a default ClientID based on the"
	elog "hardware address for interface, simply add the keyword clientid"
	elog "to dhcpcd.conf or use commandline parameter -I ''"
	elog
	elog "Also, users upgrading from 4.0 series should be aware that"
	elog "the -N, -R and -Y command line options no longer exist."
	elog "These are controled now by nohook options in dhcpcd.conf."

	# Mea culpa, feel free to remove that after some time --mgorny.
	if [[ -e "${ROOT}"/etc/systemd/system/network.target.wants/${PN}.service ]]
	then
		ebegin "Moving ${PN}.service to multi-user.target"
		mv "${ROOT}"/etc/systemd/system/network.target.wants/${PN}.service \
			"${ROOT}"/etc/systemd/system/multi-user.target.wants/
		eend ${?} \
			"Please try to re-enable dhcpcd.service"
	fi
}
