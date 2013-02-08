# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-fs/nfs-utils/nfs-utils-1.2.3.ebuild,v 1.1 2010/10/08 19:48:08 vapier Exp $

EAPI="2"

inherit eutils flag-o-matic multilib

DESCRIPTION="NFS client and server daemons"
HOMEPAGE="http://linux-nfs.org/"
SRC_URI="mirror://sourceforge/nfs/${P}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86"
IUSE="caps ipv6 kerberos +nfsv3 +nfsv4 tcpd elibc_glibc"
RESTRICT="test" #315573

# kth-krb doesn't provide the right include
# files, and nfs-utils doesn't build against heimdal either,
# so don't depend on virtual/krb.
# (04 Feb 2005 agriffis)
DEPEND_COMMON="tcpd? ( sys-apps/tcp-wrappers )
	caps? ( sys-libs/libcap )
	sys-libs/e2fsprogs-libs
	net-nds/rpcbind
	net-libs/libtirpc
	nfsv4? (
		>=dev-libs/libevent-1.0b
		>=net-libs/libnfsidmap-0.21-r1
		kerberos? (
			net-libs/librpcsecgss
			net-libs/libgssglue
			net-libs/libtirpc[kerberos]
			app-crypt/mit-krb5
		)
	)"
RDEPEND="${DEPEND_COMMON} !net-nds/portmap"
# util-linux dep is to prevent man-page collision
DEPEND="${DEPEND_COMMON}
	>=sys-apps/util-linux-2.12r-r7"

src_prepare() {
        epatch "${FILESDIR}"/${PN}-1.2.3-cross-compile.patch
}

src_configure() {
	econf \
		--with-statedir=/var/lib/nfs \
		--with-tirpcinclude=${ROOT}/usr/include/tirpc \
		--enable-tirpc \
		$(use_with tcpd tcp-wrappers) \
		$(use_enable nfsv3) \
		$(use_enable nfsv4) \
		$(use_enable ipv6) \
		$(use_enable caps) \
		$(use nfsv4 && use_enable kerberos gss || echo "--disable-gss")
}

src_compile() {
        if tc-is-cross-compiler; then
                emake CC=$(tc-getCC) || die "Failed to compile"
        else
                emake || die "Failed to compile"
        fi
}

src_install() {
	emake DESTDIR="${D}" install || die

	# Don't overwrite existing xtab/etab, install the original
	# versions somewhere safe...  more info in pkg_postinst
	keepdir /var/lib/nfs/{,sm,sm.bak}
	mv "${D}"/var/lib "${D}"/usr/$(get_libdir) || die

	# Install some client-side binaries in /sbin
	dodir /sbin
	mv "${D}"/usr/sbin/rpc.statd "${D}"/sbin/ || die

	dodoc ChangeLog README
	docinto linux-nfs ; dodoc linux-nfs/*

	insinto /etc
	doins "${FILESDIR}"/exports

	local f list="" opt_need=""
	if use nfsv4 ; then
		opt_need="rpc.idmapd"
		list="${list} rpc.idmapd rpc.pipefs"
		use kerberos && list="${list} rpc.gssd rpc.svcgssd"
	fi
	for f in nfs nfsmount rpc.statd ${list} ; do
		newinitd "${FILESDIR}"/${f}.initd ${f} || die "doinitd ${f}"
	done
	newconfd "${FILESDIR}"/nfs.confd nfs
	dosed "/^NFS_NEEDED_SERVICES=/s:=.*:=\"${opt_need}\":" /etc/conf.d/nfs #234132

	# uClibc doesn't provide rpcgen like glibc, so lets steal it from nfs-utils
	if ! use elibc_glibc ; then
		dobin tools/rpcgen/rpcgen || die "rpcgen"
		newdoc tools/rpcgen/README README.rpcgen
	fi
}

pkg_postinst() {
	# Install default xtab and friends if there's none existing.  In
	# src_install we put them in /usr/lib/nfs for safe-keeping, but
	# the daemons actually use the files in /var/lib/nfs.  #30486
	local f
	for f in "${ROOT}"/usr/$(get_libdir)/nfs/*; do
		[[ -e ${ROOT}/var/lib/nfs/${f##*/} ]] && continue
		einfo "Copying default ${f##*/} from /usr/$(get_libdir)/nfs to /var/lib/nfs"
		cp -pPR "${f}" "${ROOT}"/var/lib/nfs/
	done
}
