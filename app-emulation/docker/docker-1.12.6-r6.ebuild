# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=5

CROS_WORKON_PROJECT="coreos/docker"
CROS_WORKON_LOCALNAME="docker"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_VERSION="go1.7"

CROS_WORKON_COMMIT="a82d35e3daba1a2cd48c66e57a4f9975c39c45c6" # coreos-1.12.6
DOCKER_GITCOMMIT="${CROS_WORKON_COMMIT:0:7}"
KEYWORDS="amd64 arm64"

inherit bash-completion-r1 eutils linux-info multilib systemd udev user cros-workon coreos-go-depend

DESCRIPTION="Docker complements kernel namespacing with a high-level API which operates at the process level"
HOMEPAGE="https://dockerproject.org"
LICENSE="Apache-2.0"
SLOT="0"
IUSE="apparmor aufs +btrfs contrib +device-mapper experimental +overlay seccomp
	+selinux vim-syntax zsh-completion +journald"

# https://github.com/docker/docker/blob/master/hack/PACKAGERS.md#build-dependencies
CDEPEND="
	>=dev-db/sqlite-3.7.9:3
	device-mapper? (
		>=sys-fs/lvm2-2.02.89[thin]
	)
	seccomp? (
		>=sys-libs/libseccomp-2.2.1[static-libs]
	)
	journald? (
		>=sys-apps/systemd-225
	)
"

DEPEND="
	${CDEPEND}

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
	>=sys-apps/shadow-4.4

	=app-emulation/containerd-0.2.5-r3[seccomp?]
	=app-emulation/runc-1.0.0_rc2_p9[apparmor?,seccomp?]
"

RESTRICT="installsources strip"

# see "contrib/check-config.sh" from upstream's sources
CONFIG_CHECK="
	~NAMESPACES ~NET_NS ~PID_NS ~IPC_NS ~UTS_NS
	~CGROUPS ~CGROUP_CPUACCT ~CGROUP_DEVICE ~CGROUP_FREEZER ~CGROUP_SCHED ~CPUSETS ~MEMCG
	~KEYS ~MACVLAN ~VETH ~BRIDGE ~BRIDGE_NETFILTER
	~NF_NAT_IPV4 ~IP_NF_FILTER ~IP_NF_MANGLE ~IP_NF_TARGET_MASQUERADE
	~IP_VS ~IP_VS_RR
	~NETFILTER_XT_MATCH_ADDRTYPE ~NETFILTER_XT_MATCH_CONNTRACK
	~NETFILTER_XT_MATCH_IPVS
	~NETFILTER_XT_MARK ~NETFILTER_XT_TARGET_REDIRECT
	~NF_NAT ~NF_NAT_NEEDED

	~POSIX_MQUEUE

	~MEMCG_SWAP ~MEMCG_SWAP_ENABLED

	~BLK_CGROUP ~IOSCHED_CFQ
	~CGROUP_PERF
	~CGROUP_HUGETLB
	~NET_CLS_CGROUP
	~CFS_BANDWIDTH ~FAIR_GROUP_SCHED ~RT_GROUP_SCHED
	~XFRM_ALGO ~XFRM_USER
"

ERROR_KEYS="CONFIG_KEYS: is mandatory"
ERROR_MEMCG_SWAP="CONFIG_MEMCG_SWAP: is required if you wish to limit swap usage of containers"
ERROR_RESOURCE_COUNTERS="CONFIG_RESOURCE_COUNTERS: is optional for container statistics gathering"

ERROR_BLK_CGROUP="CONFIG_BLK_CGROUP: is optional for container statistics gathering"
ERROR_IOSCHED_CFQ="CONFIG_IOSCHED_CFQ: is optional for container statistics gathering"
ERROR_CGROUP_PERF="CONFIG_CGROUP_PERF: is optional for container statistics gathering"
ERROR_CFS_BANDWIDTH="CONFIG_CFS_BANDWIDTH: is optional for container statistics gathering"
ERROR_XFRM_ALGO="CONFIG_XFRM_ALGO: is optional for secure networks"
ERROR_XFRM_USER="CONFIG_XFRM_USER: is optional for secure networks"

pkg_setup() {
	if kernel_is lt 3 10; then
		ewarn ""
		ewarn "Using Docker with kernels older than 3.10 is unstable and unsupported."
		ewarn " - http://docs.docker.com/installation/binaries/#check-kernel-dependencies"
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

	if kernel_is le 3 18; then
		CONFIG_CHECK+="
			~RESOURCE_COUNTERS
		"
	fi

	if kernel_is le 3 13; then
		CONFIG_CHECK+="
			~NETPRIO_CGROUP
		"
	else
		CONFIG_CHECK+="
			~CGROUP_NET_PRIO
		"
	fi

	if kernel_is lt 4 5; then
		CONFIG_CHECK+="
			~MEMCG_KMEM
		"
		ERROR_MEMCG_KMEM="CONFIG_MEMCG_KMEM: is optional"
	fi

	if kernel_is lt 4 7; then
		CONFIG_CHECK+="
			~DEVPTS_MULTIPLE_INSTANCES
		"
	fi

	if use aufs; then
		CONFIG_CHECK+="
			~AUFS_FS
			~EXT4_FS_POSIX_ACL ~EXT4_FS_SECURITY
		"
		ERROR_AUFS_FS="CONFIG_AUFS_FS: is required to be set if and only if aufs-sources are used instead of aufs4/aufs3"
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

	if use seccomp; then
		CONFIG_CHECK+="
			~SECCOMP
		"
	fi

	linux-info_pkg_setup

	# create docker group for the code checking for it in /etc/group
	enewgroup docker
}

src_prepare() {
	# allow user patches (use sparingly - upstream won't support them)
	epatch_user

	# remove the .git directory so that hack/make.sh uses DOCKER_GITCOMMIT
	# for the commit hash.
	rm --recursive --force .git
}

src_compile() {
	# if we treat them right, Docker's build scripts will set up a
	# reasonable GOPATH for us
	export AUTO_GOPATH=1

	# if we're building from a zip, we need the GITCOMMIT value
	[ "$DOCKER_GITCOMMIT" ] && export DOCKER_GITCOMMIT

	if gcc-specs-pie; then
		sed -i "s/EXTLDFLAGS_STATIC='/&-fno-PIC /" hack/make.sh || die
		grep -q -- '-fno-PIC' hack/make.sh || die 'hardened sed failed'

		sed  "s/LDFLAGS_STATIC_DOCKER='/&-extldflags -fno-PIC /" \
			-i hack/make/dynbinary-client || die
		sed  "s/LDFLAGS_STATIC_DOCKER='/&-extldflags -fno-PIC /" \
			-i hack/make/dynbinary-daemon || die
		grep -q -- '-fno-PIC' hack/make/dynbinary-daemon || die 'hardened sed failed'
		grep -q -- '-fno-PIC' hack/make/dynbinary-client || die 'hardened sed failed'
	fi

	# let's set up some optional features :)
	export DOCKER_BUILDTAGS=''
	for gd in aufs btrfs device-mapper overlay; do
		if ! use $gd; then
			DOCKER_BUILDTAGS+=" exclude_graphdriver_${gd//-/}"
		fi
	done

	for tag in apparmor seccomp selinux journald; do
		if use $tag; then
			DOCKER_BUILDTAGS+=" $tag"
		fi
	done

	if has_version '<sys-fs/lvm2-2.02.110' ; then
		# Docker uses the host files when testing features, so force
		# docker to not use dm_task_deferred_remove to cover cross
		# builds.
		DOCKER_BUILDTAGS+=' libdm_no_deferred_remove'
	fi

	# https://github.com/docker/docker/pull/13338
	if use experimental; then
		export DOCKER_EXPERIMENTAL=1
	else
		unset DOCKER_EXPERIMENTAL
	fi

	go_export

	# verbose building
	export BUILDFLAGS="-x -v"

	# time to build!
	./hack/make.sh dynbinary || die 'dynbinary failed'
}

src_install() {
	VERSION="$(cat VERSION)"
	newbin "bundles/$VERSION/dynbinary-client/docker-$VERSION" docker
	newbin "bundles/$VERSION/dynbinary-daemon/dockerd-$VERSION" dockerd
	newbin "bundles/$VERSION/dynbinary-daemon/docker-proxy-$VERSION" docker-proxy
	dosym containerd /usr/bin/docker-containerd
	dosym containerd-shim /usr/bin/docker-containerd-shim
	dosym runc /usr/bin/docker-runc

	newinitd contrib/init/openrc/docker.initd docker
	newconfd contrib/init/openrc/docker.confd docker

	exeinto /usr/lib/coreos
	# Create /usr/lib/coreos/dockerd script for backwards compatibility
	doexe "${FILESDIR}/dockerd"

	systemd_dounit "${FILESDIR}/docker.service"
	systemd_dounit "${FILESDIR}/docker.socket"

	insinto /usr/lib/systemd/network
	doins "${FILESDIR}"/50-docker.network
	doins "${FILESDIR}"/90-docker-veth.network

	udev_dorules contrib/udev/*.rules

	dodoc AUTHORS CONTRIBUTING.md CHANGELOG.md NOTICE README.md
	dodoc -r docs/*

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
		# note: intentionally not using "doins" so that we preserve +x bits
		mkdir -p "${D}/usr/share/${PN}/contrib"
		cp -R contrib/* "${D}/usr/share/${PN}/contrib"
	fi
}

pkg_postinst() {
	udev_reload

	elog
	elog "To use Docker, the Docker daemon must be running as root. To automatically"
	elog "start the Docker daemon at boot, add Docker to the default runlevel:"
	elog "  rc-update add docker default"
	elog "Similarly for systemd:"
	elog "  systemctl enable docker.service"
	elog
	elog "To use Docker as a non-root user, add yourself to the 'docker' group:"
	elog "  usermod -aG docker youruser"
	elog
}
