# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

DESCRIPTION="Docker complements LXC with a high-level API which operates at the process level."
HOMEPAGE="http://www.docker.io/"

GITHUB_URI="github.com/dotcloud/docker"

if [[ ${PV} == *9999 ]]; then
	SRC_URI=""
	EGIT_REPO_URI="git://${GITHUB_URI}.git"
	inherit git-2
	KEYWORDS=""
else
	SRC_URI="https://${GITHUB_URI}/archive/v${PV}.zip -> ${P}.zip"
	DOCKER_GITCOMMIT="a1598d1"
	KEYWORDS="~amd64"
	[ "$DOCKER_GITCOMMIT" ] || die "DOCKER_GITCOMMIT must be added manually for each bump!"
fi

inherit bash-completion-r1 linux-info systemd udev user

LICENSE="Apache-2.0"
SLOT="0"
IUSE="aufs btrfs +device-mapper doc vim-syntax zsh-completion"

# TODO work with upstream to allow us to build without lvm2 installed if we have -device-mapper
CDEPEND="
	>=dev-db/sqlite-3.7.9:3
	sys-fs/lvm2[thin]
"
DEPEND="
	${CDEPEND}
	>=dev-lang/go-1.2
	>=sys-fs/btrfs-progs-0.20
	dev-vcs/git
	dev-vcs/mercurial
	doc? (
		dev-python/sphinx
		dev-python/sphinxcontrib-httpdomain
	)
"
RDEPEND="
	${CDEPEND}
	!app-emulation/docker-bin
	>=sys-apps/iproute2-3.5
	>=net-firewall/iptables-1.4
	>=app-emulation/lxc-0.8
	>=dev-vcs/git-1.7
	>=app-arch/xz-utils-4.9
	aufs? (
		|| (
			sys-fs/aufs3
			sys-kernel/aufs-sources
		)
	)
"

RESTRICT="strip"

pkg_setup() {
	CONFIG_CHECK+="
		~BRIDGE
		~IP_NF_TARGET_MASQUERADE
		~MEMCG_SWAP
		~NETFILTER_XT_MATCH_ADDRTYPE
		~NETFILTER_XT_MATCH_CONNTRACK
		~NF_NAT
		~NF_NAT_NEEDED
	"
	ERROR_MEMCG_SWAP="CONFIG_MEMCG_SWAP: is required if you wish to limit swap usage of containers"

	if use aufs; then
		CONFIG_CHECK+="
			~AUFS_FS
		"
		ERROR_AUFS_FS="CONFIG_AUFS_FS: is required to be set if and only if aufs-sources are used"
	fi

	if use btrfs; then
		CONFIG_CHECK+="
			~BTRFS_FS
		"
	fi

	if use device-mapper; then
		CONFIG_CHECK+="
			~BLK_DEV_DM
			~DM_THIN_PROVISIONING
			~EXT4_FS
		"
	fi

	check_extra_config
}

src_prepare() {
	epatch "${FILESDIR}"/fix-api-serve-until-the-acceptconnections-job.patch
}

src_compile() {
	# if we treat them right, Docker's build scripts will set up a
	# reasonable GOAPTH for us
	export AUTO_GOPATH=1

	# setup CFLAGS and LDFLAGS for separate build target
	# see https://github.com/tianon/docker-overlay/pull/10
	export CGO_CFLAGS="-I${ROOT}/usr/include"
	export CGO_LDFLAGS="-L${ROOT}/usr/lib"

	# if we're building from a zip, we need the GITCOMMIT value
	[ "$DOCKER_GITCOMMIT" ] && export DOCKER_GITCOMMIT

	if gcc-specs-pie; then
		sed -i 's/export LDFLAGS_STATIC="/export LDFLAGS_STATIC="-extldflags=-fno-PIC /' hack/make/dynbinary || die
		grep -q '-extldflags=-fno-PIC' hack/make/dynbinary || die 'sed failed'
	fi

	# time to build!
	./hack/make.sh dynbinary || die

	if use doc; then
		emake -C docs docs man || die
	fi
}

src_install() {
	VERSION=$(cat VERSION)
	newbin bundles/$VERSION/dynbinary/docker-$VERSION docker
	exeinto /usr/libexec/docker
	newexe bundles/$VERSION/dynbinary/dockerinit-$VERSION dockerinit

	newinitd contrib/init/openrc/docker.initd docker
	newconfd contrib/init/openrc/docker.confd docker

	systemd_dounit "${FILESDIR}/docker.service"
	systemd_dounit "${FILESDIR}/docker.socket"

	insinto /usr/lib/systemd/network
	doins "${FILESDIR}"/50-docker{,-veth}.network

	udev_dorules contrib/udev/*.rules

	dodoc AUTHORS CONTRIBUTING.md CHANGELOG.md NOTICE README.md
	if use doc; then
		dohtml -r docs/_build/html/*
		doman docs/_build/man/*
	fi

	dobashcomp contrib/completion/bash/*

	if use zsh-completion; then
		insinto /usr/share/zsh/site-functions
		doins contrib/completion/zsh/*
	fi

	if use vim-syntax; then
		insinto /usr/share/vim/vimfiles
		doins -r contrib/syntax/vim/ftdetect
		doins -r contrib/syntax/vim/syntax
	fi

	insinto /usr/share/${P}/contrib
	doins contrib/README
	cp -R "${S}/contrib"/* "${D}/usr/share/${P}/contrib/"
}

pkg_postinst() {
	udev_reload

	elog ""
	elog "To use docker, the docker daemon must be running as root. To automatically"
	elog "start the docker daemon at boot, add docker to the default runlevel:"
	elog "  rc-update add docker default"
	elog "Similarly for systemd:"
	elog "  systemctl enable docker.service"
	elog ""

	# create docker group if the code checking for it in /etc/group exists
	enewgroup docker

	elog "To use docker as a non-root user, add yourself to the docker group."
	elog ""
}
