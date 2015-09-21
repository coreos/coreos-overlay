# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-emulation/docker/docker-1.4.0.ebuild,v 1.1 2014/12/12 18:53:23 xarthisius Exp $

EAPI=5

DESCRIPTION="Docker complements kernel namespacing with a high-level API which operates at the process level"
HOMEPAGE="https://www.docker.com"

CROS_WORKON_PROJECT="coreos/docker"
CROS_WORKON_LOCALNAME="docker"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == *9999 ]]; then
	DOCKER_GITCOMMIT="unknown"
	KEYWORDS="~amd64 ~arm64"
elif [[ ${PV} == "1.7.1" ]]; then
	CROS_WORKON_COMMIT="a2ddd68bdbd66f6684e626dacbf6adfff758020d" # v1.7.1 with backports
	DOCKER_GITCOMMIT="${CROS_WORKON_COMMIT:0:7}"
	KEYWORDS="amd64 arm64"
else
	CROS_WORKON_COMMIT="fae44362ed4c7e161ab52f6bba6d5ee93ffbc28f" # v1.8.2 with backports
	DOCKER_GITCOMMIT="${CROS_WORKON_COMMIT:0:7}"
	KEYWORDS="amd64 ~arm64"
fi

inherit bash-completion-r1 linux-info multilib systemd udev user cros-workon

LICENSE="Apache-2.0"
SLOT="0"
IUSE="aufs +btrfs contrib +device-mapper doc experimental lxc +overlay vim-syntax zsh-completion"

# https://github.com/docker/docker/blob/master/hack/PACKAGERS.md#build-dependencies
CDEPEND="
	>=sys-kernel/coreos-kernel-3.18.0
	>=dev-db/sqlite-3.7.9:3
	device-mapper? (
		>=sys-fs/lvm2-2.02.89[thin]
	)
"

DEPEND="
	${CDEPEND}
	>=dev-lang/go-1.3
	btrfs? (
		>=sys-fs/btrfs-progs-3.16.1
	)
"

# For CoreOS builds coreos-kernel must be installed because this ebuild
# checks the kernel config. The kernel config is left by the kernel compile
# or an explicit copy when installing binary packages. See coreos-kernel.eclass
DEPEND+="sys-kernel/coreos-kernel"

# https://github.com/docker/docker/blob/master/hack/PACKAGERS.md#runtime-dependencies
# https://github.com/docker/docker/blob/master/hack/PACKAGERS.md#optional-dependencies
RDEPEND="
	${CDEPEND}

	!app-emulation/docker-bin
	>=net-firewall/iptables-1.4
	sys-process/procps
	>=dev-vcs/git-1.7
	>=app-arch/xz-utils-4.9

	lxc? (
		>=app-emulation/lxc-1.0.7
	)
	aufs? (
		|| (
			sys-fs/aufs3
			sys-kernel/aufs-sources
		)
	)
"

RESTRICT="installsources strip"

# see "contrib/check-config.sh" from upstream's sources
CONFIG_CHECK="
	~NAMESPACES ~NET_NS ~PID_NS ~IPC_NS ~UTS_NS
	~DEVPTS_MULTIPLE_INSTANCES
	~CGROUPS ~CGROUP_CPUACCT ~CGROUP_DEVICE ~CGROUP_FREEZER ~CGROUP_SCHED
	~CPUSETS
	~MACVLAN ~VETH ~BRIDGE
	~NF_NAT_IPV4 ~IP_NF_FILTER ~IP_NF_TARGET_MASQUERADE
	~NETFILTER_XT_MATCH_ADDRTYPE ~NETFILTER_XT_MATCH_CONNTRACK
	~NF_NAT ~NF_NAT_NEEDED

	~POSIX_MQUEUE

	~MEMCG_SWAP ~MEMCG_SWAP_ENABLED
	~RESOURCE_COUNTERS
	~CGROUP_PERF
	~CFS_BANDWIDTH
"

ERROR_MEMCG_SWAP="CONFIG_MEMCG_SWAP: is required if you wish to limit swap usage of containers"
ERROR_RESOURCE_COUNTERS="CONFIG_RESOURCE_COUNTERS: is optional for container statistics gathering"
ERROR_CGROUP_PERF="CONFIG_CGROUP_PERF: is optional for container statistics gathering"
ERROR_CFS_BANDWIDTH="CONFIG_CFS_BANDWIDTH: is optional for container statistics gathering"

pkg_setup() {
	if kernel_is lt 3 8; then
		eerror ""
		eerror "Using Docker with kernels older than 3.8 is unstable and unsupported."
		eerror " - http://docs.docker.com/installation/binaries/#check-kernel-dependencies"
		die 'Kernel is too old - need 3.8 or above'
	fi

	# for where these kernel versions come from, see:
	# https://www.google.com/search?q=945b2b2d259d1a4364a2799e80e8ff32f8c6ee6f+site%3Akernel.org%2Fpub%2Flinux%2Fkernel+file%3AChangeLog*
	if ! {
		kernel_is ge 3 16 \
		|| { kernel_is 3 15 && kernel_is ge 3 15 5; } \
		|| { kernel_is 3 14 && kernel_is ge 3 14 12; } \
		|| { kernel_is 3 12 && kernel_is ge 3 12 25; }
	}; then
		ewarn ""
		ewarn "There is a serious Docker-related kernel panic that has been fixed in 3.16+"
		ewarn "  (and was backported to 3.15.5+, 3.14.12+, and 3.12.25+)"
		ewarn ""
		ewarn "See also https://github.com/docker/docker/issues/2960"
	fi

	if use aufs; then
		CONFIG_CHECK+="
			~AUFS_FS
			~EXT4_FS_POSIX_ACL ~EXT4_FS_SECURITY
		"
		# TODO there must be a way to detect "sys-kernel/aufs-sources" so we don't warn "sys-fs/aufs3" users about this
		# an even better solution would be to check if the current kernel sources include CONFIG_AUFS_FS as an option, but that sounds hairy and error-prone
		ERROR_AUFS_FS="CONFIG_AUFS_FS: is required to be set if and only if aufs-sources are used"
	fi

	if use btrfs; then
		CONFIG_CHECK+="
			~BTRFS_FS
		"
	fi

	if use device-mapper; then
		CONFIG_CHECK+="
			~BLK_DEV_DM ~DM_THIN_PROVISIONING ~EXT4_FS ~EXT4_FS_POSIX_ACL ~EXT4_FS_SECURITY
		"
	fi

	if use overlay; then
		CONFIG_CHECK+="
			~OVERLAY_FS ~EXT4_FS_SECURITY ~EXT4_FS_POSIX_ACL
		"
	fi

	linux-info_pkg_setup
}

src_prepare() {
	# allow user patches (use sparingly - upstream won't support them)
	epatch_user
}

go_get_arch() {
	echo ${ARCH}
}

src_compile() {
	# if we treat them right, Docker's build scripts will set up a
	# reasonable GOPATH for us
	export AUTO_GOPATH=1

	# setup CFLAGS and LDFLAGS for separate build target
	# see https://github.com/tianon/docker-overlay/pull/10
	export CGO_CFLAGS="-I${ROOT}/usr/include"
	export CGO_LDFLAGS="-L${ROOT}/usr/$(get_libdir)"

	# if we're building from a zip, we need the GITCOMMIT value
	[ "$DOCKER_GITCOMMIT" ] && export DOCKER_GITCOMMIT

	if gcc-specs-pie; then
		sed -i "s/EXTLDFLAGS_STATIC='/EXTLDFLAGS_STATIC='-fno-PIC /" hack/make.sh || die
		grep -q -- '-fno-PIC' hack/make.sh || die 'hardened sed failed'

		sed -i "s/LDFLAGS_STATIC_DOCKER='/LDFLAGS_STATIC_DOCKER='-extldflags -fno-PIC /" hack/make/dynbinary || die
		grep -q -- '-fno-PIC' hack/make/dynbinary || die 'hardened sed failed'
	fi

	# let's set up some optional features :)
	export DOCKER_BUILDTAGS=''
	for gd in aufs btrfs device-mapper overlay; do
		if ! use $gd; then
			DOCKER_BUILDTAGS+=" exclude_graphdriver_${gd//-/}"
		fi
	done

	# https://github.com/docker/docker/pull/13338
	if use experimental; then
		export DOCKER_EXPERIMENTAL=1
	else
		unset DOCKER_EXPERIMENTAL
	fi

	export GOARCH=$(go_get_arch)
	export CGO_ENABLED=1
	export CC=$(tc-getCC)
	export CXX=$(tc-getCXX)

	# time to build!
	./hack/make.sh dynbinary || die 'dynbinary failed'

	# TODO get go-md2man and then include the man pages using docs/man/md2man-all.sh
}

src_install() {
	VERSION=$(cat VERSION)
	newbin bundles/$VERSION/dynbinary/docker-$VERSION docker
	exeinto /usr/libexec/docker
	newexe bundles/$VERSION/dynbinary/dockerinit-$VERSION dockerinit

	newinitd contrib/init/openrc/docker.initd docker
	newconfd contrib/init/openrc/docker.confd docker

	exeinto /usr/lib/coreos
	doexe "${FILESDIR}/dockerd"

	systemd_dounit "${FILESDIR}/docker.service"
	systemd_dounit "${FILESDIR}/docker.socket"
	systemd_dounit "${FILESDIR}/early-docker.service"
	systemd_dounit "${FILESDIR}/early-docker.socket"
	systemd_dounit "${FILESDIR}/early-docker.target"

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
	elog "To use Docker, the Docker daemon must be running as root. To automatically"
	elog "start the Docker daemon at boot, add Docker to the default runlevel:"
	elog "  rc-update add docker default"
	elog "Similarly for systemd:"
	elog "  systemctl enable docker.service"
	elog ""

	# create docker group if the code checking for it in /etc/group exists
	enewgroup docker

	elog "To use Docker as a non-root user, add yourself to the 'docker' group:"
	elog "  usermod -aG docker youruser"
	elog ""
}
