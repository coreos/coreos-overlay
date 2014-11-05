# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

DESCRIPTION="Docker complements kernel namespacing with a high-level API which operates at the process level."
HOMEPAGE="https://www.docker.io/"

CROS_WORKON_PROJECT="dotcloud/docker"
CROS_WORKON_LOCALNAME="docker"
CROS_WORKON_REPO="git://github.com"

GITHUB_URI="github.com/crosbymichael/docker"

if [[ ${PV} == *9999 ]]; then
	DOCKER_GITCOMMIT="deadbee"
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="4e9bbfa90054cd730e81b53b2de67a74306afc95" # v1.3.1
	DOCKER_GITCOMMIT="4e9bbfa"
	KEYWORDS="amd64"
fi

inherit bash-completion-r1 linux-info systemd udev user cros-workon

LICENSE="Apache-2.0"
SLOT="0"
IUSE="aufs +btrfs contrib +device-mapper doc lxc vim-syntax zsh-completion"

CDEPEND="
	>=dev-db/sqlite-3.7.9:3
	device-mapper? (
		sys-fs/lvm2[thin]
	)
"
DEPEND="
	${CDEPEND}
	>=dev-lang/go-1.2
	btrfs? (
		>=sys-fs/btrfs-progs-0.20
	)
	dev-vcs/git
	dev-vcs/mercurial
"
RDEPEND="
	${CDEPEND}
	!app-emulation/docker-bin
	>=net-firewall/iptables-1.4
	lxc? (
		>=app-emulation/lxc-1.0
	)
	>=dev-vcs/git-1.7
	>=app-arch/xz-utils-4.9
	aufs? (
		|| (
			sys-fs/aufs3
			sys-kernel/aufs-sources
		)
	)
"

RESTRICT="installsources strip"

pkg_setup() {
	if kernel_is lt 3 8; then
		ewarn ""
		ewarn "Using Docker with kernels older than 3.8 is unstable and unsupported."
		ewarn ""
	fi

	# many of these were borrowed from the app-emulation/lxc ebuild
	CONFIG_CHECK+="
		~CGROUPS
		~CGROUP_CPUACCT
		~CGROUP_DEVICE
		~CGROUP_FREEZER
		~CGROUP_SCHED
		~CPUSETS
		~MEMCG_SWAP
		~RESOURCE_COUNTERS

		~IPC_NS
		~NAMESPACES
		~PID_NS

		~DEVPTS_MULTIPLE_INSTANCES
		~MACVLAN
		~NET_NS
		~UTS_NS
		~VETH

		~!NETPRIO_CGROUP
		~POSIX_MQUEUE

		~BRIDGE
		~IP_NF_TARGET_MASQUERADE
		~NETFILTER_XT_MATCH_ADDRTYPE
		~NETFILTER_XT_MATCH_CONNTRACK
		~NF_NAT
		~NF_NAT_NEEDED

		~!GRKERNSEC_CHROOT_CAPS
		~!GRKERNSEC_CHROOT_CHMOD
		~!GRKERNSEC_CHROOT_DOUBLE
		~!GRKERNSEC_CHROOT_MOUNT
		~!GRKERNSEC_CHROOT_PIVOT
	"

	ERROR_MEMCG_SWAP="CONFIG_MEMCG_SWAP: is required if you wish to limit swap usage of containers"

	for c in GRKERNSEC_CHROOT_MOUNT GRKERNSEC_CHROOT_DOUBLE GRKERNSEC_CHROOT_PIVOT GRKERNSEC_CHROOT_CHMOD; do
		declare "ERROR_$c"="CONFIG_$c: see app-emulation/lxc postinst notes for why some GRSEC features make containers unusuable"
	done

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

src_compile() {
	# hack(philips): to keep the git commit from being dirty
	mv .git .git.old

	# if we treat them right, Docker's build scripts will set up a
	# reasonable GOPATH for us
	export AUTO_GOPATH=1

	# setup CFLAGS and LDFLAGS for separate build target
	# see https://github.com/tianon/docker-overlay/pull/10
	export CGO_CFLAGS="-I${ROOT}/usr/include"
	export CGO_LDFLAGS="-L${ROOT}/usr/lib"

	# if we're building from a zip, we need the GITCOMMIT value
	[ "$DOCKER_GITCOMMIT" ] && export DOCKER_GITCOMMIT

	if gcc-specs-pie; then
		sed -i "s/EXTLDFLAGS_STATIC='/EXTLDFLAGS_STATIC='-fno-PIC /" hack/make.sh || die
		grep -q -- '-fno-PIC' hack/make.sh || die 'hardened sed failed'

		sed -i 's/LDFLAGS_STATIC_DOCKER="/LDFLAGS_STATIC_DOCKER="-extldflags -fno-PIC /' hack/make/dynbinary || die
		grep -q -- '-fno-PIC' hack/make/dynbinary || die 'hardened sed failed'
	fi

	# let's set up some optional features :)
	export DOCKER_BUILDTAGS=''
	for gd in aufs btrfs device-mapper; do
		if ! use $gd; then
			DOCKER_BUILDTAGS+=" exclude_graphdriver_${gd//-/}"
		fi
	done

	# time to build!
	./hack/make.sh dynbinary || die

	# TODO pandoc the man pages using docs/man/md2man-all.sh
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
		# TODO doman contrib/man/man*/*

		docompress -x /usr/share/doc/${PF}/md
		docinto md
		dodoc -r docs/sources/*
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

	if use contrib; then
		mkdir -p "${D}/usr/share/${PN}/contrib"
		cp -R contrib/* "${D}/usr/share/${PN}/contrib"
	fi
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
