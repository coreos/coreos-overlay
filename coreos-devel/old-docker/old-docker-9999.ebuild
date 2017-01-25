# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=5

CROS_WORKON_PROJECT="coreos/docker"
CROS_WORKON_LOCALNAME="docker"
CROS_WORKON_REPO="git://github.com"

if [[ ${PV} == *9999 ]]; then
	DOCKER_GITCOMMIT="unknown"  # TODO does this still make sense?
	KEYWORDS="~amd64 -arm64"
else
	CROS_WORKON_COMMIT="98946981f575c69f923b7db49159711381b7fe8e" # v1.9.1 with backports
	DOCKER_GITCOMMIT="${CROS_WORKON_COMMIT:0:7}"
	KEYWORDS="amd64 -arm64"
fi

inherit eutils multilib cros-workon

DESCRIPTION="An old version of docker, to test backward compatibility of the current server"
HOMEPAGE="https://dockerproject.org"
LICENSE="Apache-2.0"
SLOT="0"

CDEPEND="
	~dev-lang/go-1.5.4
"

# https://github.com/docker/docker/blob/master/hack/PACKAGERS.md#runtime-dependencies
RDEPEND="
	${CDEPEND}

	!app-emulation/docker-bin
	>=dev-vcs/git-1.7
"

RESTRICT="installsources strip"

# see "contrib/check-config.sh" from upstream's sources
CONFIG_CHECK="
	~NAMESPACES ~NET_NS ~PID_NS ~IPC_NS ~UTS_NS
	~DEVPTS_MULTIPLE_INSTANCES
	~CGROUPS ~CGROUP_CPUACCT ~CGROUP_DEVICE ~CGROUP_FREEZER ~CGROUP_SCHED ~CPUSETS ~MEMCG
	~MACVLAN ~VETH ~BRIDGE ~BRIDGE_NETFILTER
	~NF_NAT_IPV4 ~IP_NF_FILTER ~IP_NF_TARGET_MASQUERADE
	~NETFILTER_XT_MATCH_ADDRTYPE ~NETFILTER_XT_MATCH_CONNTRACK
	~NF_NAT ~NF_NAT_NEEDED

	~POSIX_MQUEUE

	~MEMCG_KMEM ~MEMCG_SWAP ~MEMCG_SWAP_ENABLED

	~BLK_CGROUP ~IOSCHED_CFQ
	~CGROUP_PERF
	~CGROUP_HUGETLB
	~NET_CLS_CGROUP
	~CFS_BANDWIDTH ~FAIR_GROUP_SCHED ~RT_GROUP_SCHED
"

ERROR_MEMCG_KMEM="CONFIG_MEMCG_KMEM: is optional"
ERROR_MEMCG_SWAP="CONFIG_MEMCG_SWAP: is required if you wish to limit swap usage of containers"
ERROR_RESOURCE_COUNTERS="CONFIG_RESOURCE_COUNTERS: is optional for container statistics gathering"

ERROR_BLK_CGROUP="CONFIG_BLK_CGROUP: is optional for container statistics gathering"
ERROR_IOSCHED_CFQ="CONFIG_IOSCHED_CFQ: is optional for container statistics gathering"
ERROR_CGROUP_PERF="CONFIG_CGROUP_PERF: is optional for container statistics gathering"
ERROR_CFS_BANDWIDTH="CONFIG_CFS_BANDWIDTH: is optional for container statistics gathering"

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

	export DOCKER_BUILDTAGS=''
	for gd in aufs btrfs device-mapper overlay; do
		DOCKER_BUILDTAGS+=" exclude_graphdriver_${gd//-/}"
	done

	unset DOCKER_EXPERIMENTAL
	export GOARCH=${ARCH}
	export CGO_ENABLED=1
	export CC=$(tc-getCC)
	export CXX=$(tc-getCXX)
	export DOCKER_CLIENTONLY=1

	# Docker wants to be built inside of a docker container, so its
	# build refers directly to go, netgo, etc. We can't use coreos-go
	# or eselect, so we fake it here with the PATH, which is good enough.

	export PATH="/usr/lib/go1.5/bin:${PATH}"
	./hack/make.sh binary
}

src_install() {
	VERSION="$(cat VERSION)"
	newbin "bundles/$VERSION/binary/docker-$VERSION" old-docker
}
