# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit autotools flag-o-matic multilib systemd

DESCRIPTION="NFS client and server daemons"
HOMEPAGE="http://linux-nfs.org/"

if [[ "${PV}" = *_rc* ]] ; then
	inherit versionator
	MY_PV="$(replace_all_version_separators -)"
	SRC_URI="http://git.linux-nfs.org/?p=steved/nfs-utils.git;a=snapshot;h=refs/tags/${PN}-${MY_PV};sf=tgz -> ${P}.tar.gz"
	S="${WORKDIR}/${PN}-${PN}-${MY_PV}"
else
	SRC_URI="mirror://sourceforge/nfs/${P}.tar.bz2"
	KEYWORDS="alpha amd64 arm arm64 hppa ia64 ~mips ppc ppc64 s390 sh sparc x86"
fi

LICENSE="GPL-2"
SLOT="0"
IUSE="caps ipv6 kerberos ldap +libmount nfsdcld +nfsidmap +nfsv4 nfsv41 selinux tcpd +uuid"
REQUIRED_USE="kerberos? ( nfsv4 )"
RESTRICT="test" #315573

# kth-krb doesn't provide the right include
# files, and nfs-utils doesn't build against heimdal either,
# so don't depend on virtual/krb.
# (04 Feb 2005 agriffis)
DEPEND_COMMON="
	net-libs/libtirpc:=
	>=net-nds/rpcbind-0.2.4
	sys-libs/e2fsprogs-libs
	caps? ( sys-libs/libcap )
	ldap? ( net-nds/openldap )
	libmount? ( sys-apps/util-linux )
	nfsdcld? ( >=dev-db/sqlite-3.3 )
	nfsv4? (
		dev-libs/libevent:=
		>=sys-apps/keyutils-1.5.9
		kerberos? (
			>=net-libs/libtirpc-0.2.4-r1[kerberos]
			app-crypt/mit-krb5
		)
	)
	nfsv41? (
		sys-fs/lvm2
	)
	tcpd? ( sys-apps/tcp-wrappers )
	uuid? ( sys-apps/util-linux )"
RDEPEND="${DEPEND_COMMON}
	!net-libs/libnfsidmap
	!net-nds/portmap
	!<sys-apps/openrc-0.13.9
	selinux? (
		sec-policy/selinux-rpc
		sec-policy/selinux-rpcbind
	)
"
DEPEND="${DEPEND_COMMON}
	net-libs/rpcsvc-proto
	virtual/pkgconfig"

PATCHES=(
	"${FILESDIR}"/${PN}-1.1.4-mtab-sym.patch
	"${FILESDIR}"/${PN}-1.2.8-cross-build.patch
	"${FILESDIR}"/${P}-svcgssd_undefined_reference.patch #641912
)

src_prepare() {
	default

	sed \
		-e "/^sbindir/s:= := \"${EPREFIX}\":g" \
		-i utils/*/Makefile.am || die

	eautoreconf
}

src_configure() {
	export libsqlite3_cv_is_recent=yes # Our DEPEND forces this.
	export ac_cv_header_keyutils_h=$(usex nfsidmap)
	local myeconfargs=(
		--with-statedir="${EPREFIX%/}"/var/lib/nfs
		--enable-tirpc
		--with-tirpcinclude="${EPREFIX%/}"/usr/include/tirpc/
		--with-pluginpath="${EPREFIX%/}"/usr/$(get_libdir)/libnfsidmap
		--with-rpcgen
		--with-systemd="$(systemd_get_systemunitdir)"
		--without-gssglue
		$(use_enable caps)
		$(use_enable ipv6)
		$(use_enable kerberos gss)
		$(use_enable kerberos svcgss)
		$(use_enable ldap)
		$(use_enable libmount libmount-mount)
		$(use_enable nfsdcld nfsdcltrack)
		$(use_enable nfsv4)
		$(use_enable nfsv41)
		$(use_enable uuid)
		$(use_with tcpd tcp-wrappers)
	)
	econf "${myeconfargs[@]}"
}

src_compile(){
	# remove compiled files bundled in the tarball
	emake clean
	default
}

src_install() {
	default
	rm linux-nfs/Makefile* || die
	dodoc -r linux-nfs README

	# Don't overwrite existing xtab/etab, install the original
	# versions somewhere safe...  more info in pkg_postinst
	keepdir /var/lib/nfs/{,sm,sm.bak}
	mv "${ED%/}"/var/lib/nfs "${ED%/}"/usr/$(get_libdir)/ || die

	if use nfsv4 && use nfsidmap ; then
		# Install a config file for idmappers in newer kernels. #415625
		insinto /etc/request-key.d
		echo 'create id_resolver * * /usr/sbin/nfsidmap -t 600 %k %d' > id_resolver.conf
		doins id_resolver.conf
	fi

	systemd_dotmpfilesd "${FILESDIR}"/nfs-utils.conf

	# Provide an empty xtab for compatibility with the old tmpfiles config.
	touch "${ED%/}"/usr/$(get_libdir)/nfs/xtab

	# Maintain compatibility with the old gentoo systemd unit names, since nfs-utils has units upstream now.
	dosym nfs-server.service "$(systemd_get_systemunitdir)"/nfsd.service
	dosym nfs-idmapd.service "$(systemd_get_systemunitdir)"/rpc-idmapd.service
	dosym nfs-mountd.service "$(systemd_get_systemunitdir)"/rpc-mountd.service
}
