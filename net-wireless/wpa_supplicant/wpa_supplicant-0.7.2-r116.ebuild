# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-wireless/wpa_supplicant/wpa_supplicant-0.7.0.ebuild,v 1.7 2009/07/24 16:42:43 josejx Exp $

EAPI="2"
CROS_WORKON_COMMIT="728b68f811a2b0b12ea57c2e5386bee7e36f0bf9"
CROS_WORKON_TREE="6fa69fc25b9ed779d0e60b293e3b7c40edc95bb5"
CROS_WORKON_PROJECT="chromiumos/third_party/hostap"

inherit eutils toolchain-funcs qt3 qt4 cros-workon

DESCRIPTION="IEEE 802.1X/WPA supplicant for secure wireless transfers"
HOMEPAGE="http://hostap.epitest.fi/wpa_supplicant/"
#SRC_URI="http://hostap.epitest.fi/releases/${P}.tar.gz"
LICENSE="|| ( GPL-2 BSD )"

SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="dbus debug gnutls eap-sim madwifi ps3 qt3 qt4 readline ssl wps kernel_linux kernel_FreeBSD"

DEPEND="dev-libs/libnl:0
	dbus? ( sys-apps/dbus )
	kernel_linux? (
		eap-sim? ( sys-apps/pcsc-lite )
		madwifi? ( ||
			( >net-wireless/madwifi-ng-tools-0.9.3
			net-wireless/madwifi-old )
		)
	)
	!kernel_linux? ( net-libs/libpcap )
	qt4? ( x11-libs/qt-gui:4
		x11-libs/qt-svg:4 )
	!qt4? ( qt3? ( x11-libs/qt:3 ) )
	readline? ( sys-libs/ncurses sys-libs/readline )
	ssl? ( dev-libs/openssl chromeos-base/chaps dev-libs/engine_pkcs11 )
	!ssl? ( gnutls? ( net-libs/gnutls ) )
	!ssl? ( !gnutls? ( dev-libs/libtommath ) )"
RDEPEND="${DEPEND}"

MY_S="${WORKDIR}/${P}/wpa_supplicant"

pkg_setup() {
	if use gnutls && use ssl ; then
		einfo "You have both 'gnutls' and 'ssl' USE flags enabled: defaulting to USE=\"ssl\""
	fi

	if use qt3 && use qt4 ; then
		einfo "You have both 'qt3' and 'qt4' USE flags enabled: defaulting to USE=\"qt4\""
	fi
}

src_prepare() {
	cd ${MY_S}
	# net/bpf.h needed for net-libs/libpcap on Gentoo/FreeBSD
	sed -i \
		-e "s:\(#include <pcap\.h>\):#include <net/bpf.h>\n\1:" \
		../src/l2_packet/l2_packet_freebsd.c || die

	# People seem to take the example configuration file too literally (bug #102361)
	sed -i \
		-e "s:^\(opensc_engine_path\):#\1:" \
		-e "s:^\(pkcs11_engine_path\):#\1:" \
		-e "s:^\(pkcs11_module_path\):#\1:" \
		wpa_supplicant.conf || die

	# Change configuration to match Gentoo locations (bug #143750)
	sed -i \
		-e "s:/usr/lib/opensc:/usr/$(get_libdir):" \
		-e "s:/usr/lib/pkcs11:/usr/$(get_libdir):" \
		wpa_supplicant.conf || die
}

src_configure() {
	local CFGFILE=${MY_S}/.config

	# Toolchain setup
	echo "CC = $(tc-getCC)" > ${CFGFILE}

	# Build w/ debug symbols
	echo "CFLAGS += -ggdb" >> ${CFGFILE}

	# Basic setup
	echo "CONFIG_CTRL_IFACE=y" >> ${CFGFILE}
	echo "CONFIG_BACKEND=file" >> ${CFGFILE}

	# Basic authentication methods
	# NOTE: These are the options set by the Chromium OS build
	echo "CONFIG_DYNAMIC_EAP_METHODS=y" >> ${CFGFILE}
	echo "CONFIG_IEEE8021X_EAPOL=y" >> ${CFGFILE}
	echo "CONFIG_EAP_MD5=y" >> ${CFGFILE}
	echo "CONFIG_EAP_MSCHAPV2=y" >> ${CFGFILE}
	echo "CONFIG_EAP_TLS=y" >> ${CFGFILE}
	echo "CONFIG_EAP_PEAP=y" >> ${CFGFILE}
	echo "CONFIG_EAP_TTLS=y" >> ${CFGFILE}
	echo "CONFIG_EAP_GTC=y" >> ${CFGFILE}
	echo "CONFIG_EAP_OTP=y" >> ${CFGFILE}
	echo "CONFIG_EAP_LEAP=y" >> ${CFGFILE}
	echo "CONFIG_PKCS12=y" >> ${CFGFILE}
	echo "CONFIG_PEERKEY=y" >> ${CFGFILE}
	echo "CONFIG_BGSCAN_SIMPLE=y" >> ${CFGFILE}
	echo "CONFIG_BGSCAN_LEARN=y" >> ${CFGFILE}
	echo "CONFIG_BGSCAN_DELTA=y" >> ${CFGFILE}
	echo "CONFIG_IEEE80211W=y" >> ${CFGFILE}

	if use dbus ; then
		echo "CONFIG_CTRL_IFACE_DBUS_NEW=y" >> ${CFGFILE}
		echo "CONFIG_CTRL_IFACE_DBUS_INTRO=y" >> ${CFGFILE}
	fi

	if use debug ; then
		echo "CONFIG_DEBUG_SYSLOG=y" >> ${CFGFILE}
		echo "CONFIG_DEBUG_SYSLOG_FACILITY=LOG_LOCAL6" >> ${CFGFILE}
	fi

	if use eap-sim ; then
		# Smart card authentication
		echo "CONFIG_EAP_SIM=y"       >> ${CFGFILE}
		echo "CONFIG_EAP_AKA=y"       >> ${CFGFILE}
		echo "CONFIG_EAP_AKA_PRIME=y" >> ${CFGFILE}
		echo "CONFIG_PCSC=y"          >> ${CFGFILE}
	fi

	if use readline ; then
		# readline/history support for wpa_cli
		echo "CONFIG_READLINE=y" >> ${CFGFILE}
	fi

	# SSL authentication methods
	if use ssl ; then
		echo "CONFIG_TLS=openssl"    >> ${CFGFILE}
		echo "CONFIG_SMARTCARD=y"    >> ${CFGFILE}
	elif use gnutls ; then
		echo "CONFIG_TLS=gnutls"     >> ${CFGFILE}
		echo "CONFIG_GNUTLS_EXTRA=y" >> ${CFGFILE}
	else
		echo "CONFIG_TLS=internal"   >> ${CFGFILE}
	fi

	if use kernel_linux ; then
		# Linux specific drivers
		#echo "CONFIG_DRIVER_ATMEL=y"       >> ${CFGFILE}
		#echo "CONFIG_DRIVER_BROADCOM=y"   >> ${CFGFILE}
		#echo "CONFIG_DRIVER_HERMES=y"	   >> ${CFGFILE}
		#echo "CONFIG_DRIVER_HOSTAP=y"      >> ${CFGFILE}
		#echo "CONFIG_DRIVER_IPW=y"         >> ${CFGFILE}
		#echo "CONFIG_DRIVER_NDISWRAPPER=y" >> ${CFGFILE}
		echo "CONFIG_DRIVER_NL80211=y"     >> ${CFGFILE}
		#echo "CONFIG_DRIVER_PRISM54=y"    >> ${CFGFILE}
		#echo "CONFIG_DRIVER_RALINK=y"      >> ${CFGFILE}
		echo "CONFIG_DRIVER_WEXT=y"        >> ${CFGFILE}
		echo "CONFIG_DRIVER_WIRED=y"       >> ${CFGFILE}

		if use madwifi ; then
			# Add include path for madwifi-driver headers
			echo "CFLAGS += -I/usr/include/madwifi" >> ${CFGFILE}
			echo "CONFIG_DRIVER_MADWIFI=y"          >> ${CFGFILE}
		fi

		if use ps3 ; then
			echo "CONFIG_DRIVER_PS3=y" >> ${CFGFILE}
		fi

	elif use kernel_FreeBSD ; then
		# FreeBSD specific driver
		echo "CONFIG_DRIVER_BSD=y" >> ${CFGFILE}
	fi

	# Wi-Fi Protected Setup (WPS)
	if use wps ; then
		echo "CONFIG_WPS=y" >> ${CFGFILE}
	fi

	# Enable mitigation against certain attacks against TKIP
	echo "CONFIG_DELAYED_MIC_ERROR_REPORT=y" >> ${CFGFILE}
}

src_compile() {
	emake -C wpa_supplicant || die "emake failed"

	if use qt4 ; then
		cd "${MY_S}"/wpa_gui-qt4
		eqmake4 wpa_gui.pro
		emake || die "Qt4 wpa_gui compilation failed"
	elif use qt3 ; then
		cd "${MY_S}"/wpa_gui
		eqmake3 wpa_gui.pro
		emake || die "Qt3 wpa_gui compilation failed"
	fi
}

src_install() {
	cd ${MY_S}
	dosbin wpa_supplicant || die
	dobin wpa_cli wpa_passphrase || die

	# baselayout-1 compat
	if has_version "<sys-apps/baselayout-2.0.0"; then
		dodir /sbin
		dosym /usr/sbin/wpa_supplicant /sbin/wpa_supplicant || die
		dodir /bin
		dosym /usr/bin/wpa_cli /bin/wpa_cli || die
	fi

	exeinto /etc/wpa_supplicant/

	dodoc ChangeLog {eap_testing,todo}.txt README{,-WPS} \
		wpa_supplicant.conf || die "dodoc failed"

	# TODO(chromium): Had to comment this out. Why?
	#	doman doc/docbook/*.{5,8} || die "doman failed"

	if use qt4 ; then
		into /usr
		dobin wpa_gui-qt4/wpa_gui || die
	elif use qt3 ; then
		into /usr
		dobin wpa_gui/wpa_gui || die
	fi

	if use qt3 || use qt4 ; then
		doicon wpa_gui-qt4/icons/wpa_gui.svg || die "Icon not found"
		make_desktop_entry wpa_gui "WPA Supplicant Administration GUI" "wpa_gui" "Qt;Network;"
	fi

	if use dbus ; then
		insinto /etc/dbus-1/system.d
		newins dbus/dbus-wpa_supplicant.conf wpa_supplicant.conf || die
		insinto /usr/share/dbus-1/interfaces
		doins dbus/fi.w1.wpa_supplicant1.xml || die
		insinto /usr/share/dbus-1/system-services
		newins dbus/fi.w1.wpa_supplicant1.service 'fi.w1.wpa_supplicant1.service' || die
		keepdir /var/run/wpa_supplicant
	fi
}

pkg_postinst() {
	einfo "If this is a clean installation of wpa_supplicant, you"
	einfo "have to create a configuration file named"
	einfo "/etc/wpa_supplicant/wpa_supplicant.conf"
	einfo
	einfo "An example configuration file is available for reference in"
	einfo "/usr/share/doc/${PF}/"

	if [[ -e ${ROOT}etc/wpa_supplicant.conf ]] ; then
		echo
		ewarn "WARNING: your old configuration file ${ROOT}etc/wpa_supplicant.conf"
		ewarn "needs to be moved to ${ROOT}etc/wpa_supplicant/wpa_supplicant.conf"
	fi

	if use madwifi ; then
		echo
		einfo "This package compiles against the headers installed by"
		einfo "madwifi-old, madwifi-ng or madwifi-ng-tools."
		einfo "You should re-emerge ${PN} after upgrading these packages."
	fi
}
