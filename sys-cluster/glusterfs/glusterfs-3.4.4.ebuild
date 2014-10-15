# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-cluster/glusterfs/glusterfs-3.4.4.ebuild,v 1.2 2014/08/11 22:28:30 blueness Exp $

EAPI=5

PYTHON_COMPAT=( python{2_6,2_7} )
AUTOTOOLS_AUTORECONF=1

inherit autotools-utils elisp-common eutils multilib python-single-r1 versionator

DESCRIPTION="GlusterFS is a powerful network/cluster filesystem"
HOMEPAGE="http://www.gluster.org/"
SRC_URI="http://download.gluster.org/pub/gluster/${PN}/$(get_version_component_range '1-2')/${PV}/${P}.tar.gz"

LICENSE="|| ( GPL-2 LGPL-3+ )"
SLOT="0"
KEYWORDS="~amd64 ~ppc ~ppc64 ~x86"
IUSE="bd-xlator debug emacs extras +fuse +georeplication infiniband static-libs systemtap vim-syntax"

REQUIRED_USE="georeplication? ( ${PYTHON_REQUIRED_USE} )"

RDEPEND="bd-xlator? ( sys-fs/lvm2 )
	emacs? ( virtual/emacs )
	fuse? ( >=sys-fs/fuse-2.7.0 )
	georeplication? ( ${PYTHON_DEPS} )
	infiniband? ( sys-infiniband/libibverbs sys-infiniband/librdmacm )
	systemtap? ( dev-util/systemtap )
	sys-libs/readline
	dev-libs/libaio
	dev-libs/libxml2
	dev-libs/openssl
	|| ( sys-libs/glibc sys-libs/argp-standalone )"
DEPEND="${RDEPEND}
	virtual/pkgconfig
	sys-devel/bison
	sys-devel/flex"

SITEFILE="50${PN}-mode-gentoo.el"

PATCHES=(
	"${FILESDIR}/${PN}-3.4.0-silent_rules.patch"
	"${FILESDIR}/${PN}-3.4.0-build-shared-only.patch"
)

DOCS=( AUTHORS ChangeLog NEWS README THANKS )

# Maintainer notes:
# * The build system will always configure & build argp-standalone but it'll never use it
#   if the argp.h header is found in the system. Which should be the case with
#   glibc or if argp-standalone is installed.

pkg_setup() {
	 use georeplication && python-single-r1_pkg_setup
}

src_configure() {
	local myeconfargs=(
		--disable-dependency-tracking
		--disable-silent-rules
		--disable-fusermount
		$(use_enable debug)
		$(use_enable bd-xlator )
		$(use_enable fuse fuse-client)
		$(use_enable georeplication)
		$(use_enable infiniband ibverbs)
		$(use_enable static-libs static)
		$(use_enable systemtap)
		--docdir=/usr/share/doc/${PF}
		--localstatedir=/var
	)
	autotools-utils_src_configure
}

src_compile() {
	autotools-utils_src_compile

	use emacs && elisp-compile extras/glusterfs-mode.el
}

src_install() {
	autotools-utils_src_install

	rm "${D}/etc/glusterfs/glusterfs-logrotate" || die "removing false logrotate failed"
	insinto /etc/logrotate.d
	newins "${FILESDIR}"/glusterfs.logrotate glusterfs

	if use emacs ; then
		elisp-install ${PN} extras/glusterfs-mode.el*
		elisp-site-file-install "${FILESDIR}/${SITEFILE}"
	fi

	if use vim-syntax ; then
		insinto /usr/share/vim/vimfiles/ftdetect; doins "${FILESDIR}"/${PN}.vim
		insinto /usr/share/vim/vimfiles/syntax; doins extras/${PN}.vim
	fi

	if use extras ; then
		sed -i -e "s|quota-remove-xattr.sh|${PN}-quota-remove-xattr|" extras/quota-metadata-cleanup.sh || die "sed failed"
		for e in backend-xattr-sanitize backend-cleanup migrate-unify-to-distribute quota-metadata-cleanup quota-remove-xattr ; do
			newbin extras/${e}.sh ${PN}-${e}
		done
		newbin extras/disk_usage_sync.sh ${PN}-disk-usage-sync
	fi

	newinitd "${FILESDIR}/${PN}-r1.initd" glusterfsd
	newinitd "${FILESDIR}/glusterd-r1.initd" glusterd
	newconfd "${FILESDIR}/${PN}.confd" glusterfsd

	keepdir /var/log/${PN}
	keepdir /var/lib/glusterd

	# QA
	rm -rf "${ED}/var/run/"

	use georeplication && python_fix_shebang "${ED}"
}

pkg_postinst() {
	elog "Starting with ${PN}-3.1.0, you can use the glusterd daemon to configure your"
	elog "volumes dynamically. To do so, simply use the gluster CLI after running:"
	elog "  /etc/init.d/glusterd start"
	elog
	elog "For static configurations, the glusterfsd startup script can be multiplexed."
	elog "The default startup script uses /etc/conf.d/glusterfsd to configure the"
	elog "separate service.  To create additional instances of the glusterfsd service"
	elog "simply create a symlink to the glusterfsd startup script."
	elog
	elog "Example:"
	elog "    # ln -s glusterfsd /etc/init.d/glusterfsd2"
	elog "    # ${EDITOR} /etc/glusterfs/glusterfsd2.vol"
	elog "You can now treat glusterfsd2 like any other service"
	elog
	ewarn "You need to use a ntp client to keep the clocks synchronized across all"
	ewarn "of your servers. Setup a NTP synchronizing service before attempting to"
	ewarn "run GlusterFS."

	elog
	elog "If you are upgrading from a previous version of ${PN}, please read:"
	elog "  https://vbellur.wordpress.com/2013/07/15/upgrading-to-glusterfs-3-4/"

	use emacs && elisp-site-regen
}

pkg_postrm() {
	use emacs && elisp-site-regen
}
